/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2026 Synergy App Ltd
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "OfflineActivation.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace synergy::license {

namespace {

// The wire format here must stay in lockstep with the vendor signer in the web-muskoka repo
// (website lib/server/product-license/offline-activation-protocol.ts).
constexpr auto kDomain = std::string_view{"synergy-offline-activation-v1"};
constexpr size_t kSecretLength = 32;

// No vowels, so codes can never spell words; no I, L, or O, so nothing is mistaken
// for 1 or 0 (and typed O/I/L are forgiven by mapping them to digits).
constexpr auto kCodeAlphabet = std::string_view{"0123456789BCDFGHJKMNPQRSTVWXYZ"};

constexpr std::uint64_t kCodeAlphabetSize = 30;
constexpr size_t kCodeLength = 12;
constexpr size_t kFingerprintDigits = 2;
constexpr size_t kMachineDigits = 10;

constexpr std::uint64_t codeSpace(size_t digits)
{
  std::uint64_t value = 1;
  for (size_t i = 0; i < digits; ++i) {
    value *= kCodeAlphabetSize;
  }
  return value;
}

constexpr auto kFingerprintRange = codeSpace(kFingerprintDigits);
constexpr auto kMachineRange = codeSpace(kMachineDigits);
constexpr auto kResponseRange = codeSpace(kCodeLength);

using Bytes = std::vector<std::uint8_t>;

std::string encodeBase30(std::uint64_t value, size_t digits)
{
  std::string out(digits, '0');
  for (size_t i = digits; i > 0; --i) {
    out[i - 1] = kCodeAlphabet[value % kCodeAlphabetSize];
    value /= kCodeAlphabetSize;
  }
  return out;
}

std::uint64_t bytesToUint(const Bytes &bytes, size_t count)
{
  std::uint64_t value = 0;
  for (size_t i = 0; i < count; ++i) {
    value = (value << 8) | bytes[i];
  }
  return value;
}

constexpr auto kAsciiWhitespace = std::string_view{" \t\n\r\f\v"};

char asciiUpper(char c)
{
  return (c >= 'a' && c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : c;
}

char forgiveConfusable(char c)
{
  if (c == 'O') {
    return '0';
  }
  if (c == 'I' || c == 'L') {
    return '1';
  }
  return c;
}

std::optional<std::string> canonicalizeCode(const std::string &code)
{
  std::string out;
  for (const auto c : code) {
    if (c == '-' || kAsciiWhitespace.find(c) != std::string_view::npos) {
      continue;
    }
    const auto upper = forgiveConfusable(asciiUpper(c));
    if (kCodeAlphabet.find(upper) == std::string_view::npos) {
      return std::nullopt;
    }
    out.push_back(upper);
  }
  return out;
}

std::optional<Bytes> hexDecode(const std::string &hex)
{
  if (hex.length() % 2 != 0) {
    return std::nullopt;
  }
  const auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
      return c - 'A' + 10;
    }
    return -1;
  };
  Bytes out;
  out.reserve(hex.length() / 2);
  for (size_t i = 0; i < hex.length(); i += 2) {
    const auto high = nibble(hex[i]);
    const auto low = nibble(hex[i + 1]);
    if (high < 0 || low < 0) {
      return std::nullopt;
    }
    out.push_back(static_cast<std::uint8_t>((high << 4) | low));
  }
  return out;
}

Bytes sha256(const Bytes &data)
{
  Bytes digest(EVP_MAX_MD_SIZE);
  unsigned int length = 0;
  if (EVP_Digest(data.data(), data.size(), digest.data(), &length, EVP_sha256(), nullptr) != 1) {
    return {};
  }
  digest.resize(length);
  return digest;
}

void append(Bytes &bytes, std::string_view text)
{
  bytes.insert(bytes.end(), text.begin(), text.end());
}

std::string canonicalizeSerial(const std::string &serialHex)
{
  const auto first = serialHex.find_first_not_of(kAsciiWhitespace);
  if (first == std::string::npos) {
    return {};
  }
  const auto last = serialHex.find_last_not_of(kAsciiWhitespace);
  auto serial = serialHex.substr(first, last - first + 1);
  std::transform(serial.begin(), serial.end(), serial.begin(), [](char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
  });
  return serial;
}

Bytes hmacSha256(const Bytes &key, const Bytes &data)
{
  Bytes digest(EVP_MAX_MD_SIZE);
  unsigned int length = 0;
  const auto result =
      HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), data.data(), data.size(), digest.data(), &length);
  if (result == nullptr) {
    return {};
  }
  digest.resize(length);
  return digest;
}

} // namespace

std::string buildOfflineChallenge(const std::string &machineId, const std::string &serialHex)
{
  if (machineId.empty()) {
    return {};
  }

  const auto serial = canonicalizeSerial(serialHex);

  Bytes serialBytes;
  append(serialBytes, serial);
  const auto serialDigest = sha256(serialBytes);

  Bytes message;
  append(message, kDomain);
  message.push_back(0);
  append(message, machineId);
  message.push_back(0);
  append(message, serial);
  const auto machineDigest = sha256(message);

  if (serialDigest.size() < 4 || machineDigest.size() < 8) {
    return {};
  }

  const auto fingerprint = bytesToUint(serialDigest, 4) % kFingerprintRange;
  const auto machineCode = bytesToUint(machineDigest, 8) % kMachineRange;
  return encodeBase30(fingerprint, kFingerprintDigits) + encodeBase30(machineCode, kMachineDigits);
}

bool verifyOfflineResponse(const std::string &machineId, const std::string &serialHex, const std::string &responseCode)
{
  return verifyOfflineResponse(machineId, serialHex, responseCode, kOfflineActivationHex);
}

bool verifyOfflineResponse(
    const std::string &machineId, const std::string &serialHex, const std::string &responseCode,
    const std::string &secretHex
)
{
  if (machineId.empty()) {
    return false;
  }

  const auto secret = hexDecode(secretHex);
  if (!secret.has_value() || secret->size() != kSecretLength) {
    return false;
  }

  const auto canonicalResponse = canonicalizeCode(responseCode);
  if (!canonicalResponse.has_value() || canonicalResponse->length() != kCodeLength) {
    return false;
  }

  const auto challenge = buildOfflineChallenge(machineId, serialHex);
  if (challenge.empty()) {
    return false;
  }

  Bytes message;
  append(message, kDomain);
  message.push_back(':');
  append(message, challenge);
  const auto mac = hmacSha256(*secret, message);
  if (mac.size() < 8) {
    return false;
  }

  const auto expected = encodeBase30(bytesToUint(mac, 8) % kResponseRange, kCodeLength);
  return CRYPTO_memcmp(expected.data(), canonicalResponse->data(), kCodeLength) == 0;
}

std::string formatOfflineCode(const std::string &code, int groupSize)
{
  if (groupSize <= 0) {
    return code;
  }
  std::string out;
  for (size_t i = 0; i < code.length(); ++i) {
    if (i > 0 && i % static_cast<size_t>(groupSize) == 0) {
      out.push_back('-');
    }
    out.push_back(code[i]);
  }
  return out;
}

} // namespace synergy::license
