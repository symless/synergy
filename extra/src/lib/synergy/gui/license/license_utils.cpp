/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2024 - 2026 Synergy App Ltd
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

#include "license_utils.h"

#include "synergy/gui/TestSettings.h"
#include "synergy/license/parse_serial_key.h"

#include <QString>
#include <QtCore>

namespace synergy::gui::license {

#ifdef SYNERGY_ENABLE_ACTIVATION
const bool kEnableActivation = true;
#else
const bool kEnableActivation = false;
#endif // SYNERGY_ENABLE_ACTIVATION

bool isActivationEnabled()
{
  return synergy::gui::TestSettings::instance().isLicensingEnabled() || kEnableActivation;
}

synergy::license::SerialKey parseSerialKey(const QString &hexString)
{
  try {
    return synergy::license::parseSerialKey(hexString.toStdString());
  } catch (const std::exception &e) {
    qWarning("failed to parse serial key: %s", e.what());
    return synergy::license::SerialKey::invalid();
  } catch (...) {
    qWarning("failed to parse serial key, unknown error");
    return synergy::license::SerialKey::invalid();
  }
}

} // namespace synergy::gui::license
