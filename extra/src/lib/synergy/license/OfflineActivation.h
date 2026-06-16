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

#pragma once

#include <string>

namespace synergy::license {

/**
 * Challenge-response activation for air-gapped machines. The machine shows a
 * short challenge code derived from its hardware id and serial key, the customer
 * exchanges it for a response code on their account page (web-muskoka repo), and
 * the machine verifies the response with no network access. Both codes are
 * hand-typed, so the response is a short truncated MAC rather than a signature;
 * extracting the embedded secret defeats it, which is an accepted trade-off
 * (deterrence, not copy protection).
 */

/// @brief Shared hex the response codes are verified against (deliberately not secret)
inline constexpr auto kOfflineActivationHex = "8acfc45b6524b47ae91e68610c7d2463c28b7e5f80a04d3fd589c87917848534";

/**
 * @brief Builds the challenge code the customer enters on their account page.
 * @param machineId A stable hardware identifier for this machine.
 * @param serialHex The license serial key as a hex string.
 * @return Canonical 12-character challenge code (use @ref formatOfflineCode for display).
 */
std::string buildOfflineChallenge(const std::string &machineId, const std::string &serialHex);

/**
 * @brief Verifies a response code against the embedded shared secret.
 * @param machineId Must match the machine the challenge was built on.
 * @param serialHex Must match the serial key the challenge was built with.
 * @param responseCode The response code from the account page; separators and letter case are ignored.
 * @return True only if the response is valid for this machine and serial.
 */
bool verifyOfflineResponse(const std::string &machineId, const std::string &serialHex, const std::string &responseCode);

/**
 * @brief Same as the three-argument overload, with an explicit secret for tests.
 * @param secretHex Shared secret, 64 hex characters.
 */
bool verifyOfflineResponse(
    const std::string &machineId, const std::string &serialHex, const std::string &responseCode,
    const std::string &secretHex
);

/**
 * @brief Inserts dash separators for display, e.g. "AAAABBBB" with group size 4 becomes "AAAA-BBBB".
 */
std::string formatOfflineCode(const std::string &code, int groupSize);

} // namespace synergy::license
