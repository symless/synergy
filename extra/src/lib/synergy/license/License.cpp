/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2016 - 2026 Synergy App Ltd
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

#include "License.h"

#include "Product.h"
#include "synergy/license/SerialKey.h"
#include "synergy/license/parse_serial_key.h"

#include <chrono>

using namespace std::chrono;

namespace synergy::license {

License::License(const std::string &hexString) : m_serialKey(parseSerialKey(hexString))
{
}

License::License(const SerialKey &serialKey) : m_serialKey(serialKey)
{
  if (!m_serialKey.isValid) {
    throw InvalidSerialKey();
  }
}

bool License::isTrial() const
{
  return m_serialKey.type.isTrial();
}

bool License::isSubscription() const
{
  return m_serialKey.type.isSubscription();
}

bool License::isTimeLimited() const
{
  return m_serialKey.type.isSubscription() || m_serialKey.type.isTrial();
}

bool License::isTlsAvailable() const
{
  return m_serialKey.product.isFeatureAvailable(Product::Feature::kTls);
}

bool License::isSettingsScopeAvailable() const
{
  return m_serialKey.product.isFeatureAvailable(Product::Feature::kSettingsScope);
}

Product::Edition License::productEdition() const
{
  return m_serialKey.product.edition();
}

bool License::isExpiringSoon() const
{
  if (!isTimeLimited()) {
    return false;
  }

  if (!m_serialKey.warnTime.has_value()) {
    throw NoTimeLimitError();
  }

  return m_nowFunc() >= m_serialKey.warnTime.value();
}

bool License::isExpired() const
{
  if (!isTimeLimited()) {
    return false;
  }

  if (!m_serialKey.expireTime.has_value()) {
    throw NoTimeLimitError();
  }

  return m_nowFunc() >= m_serialKey.expireTime.value();
}

seconds License::secondsLeft() const
{
  if (!m_serialKey.expireTime.has_value()) {
    throw NoTimeLimitError();
  }

  auto expireTime = m_serialKey.expireTime.value();

  auto timeLeft = expireTime - m_nowFunc();
  return duration_cast<seconds>(timeLeft);
}

days License::daysLeft() const
{
  return duration_cast<days>(secondsLeft());
}

std::string License::productName() const
{
  auto name = m_serialKey.product.name();
  if (m_serialKey.type.isTrial()) {
    name += " (Trial)";
  }
  return name;
}

} // namespace synergy::license
