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

#include <QString>
#include <QVariant>

#include <optional>
#include <utility>

namespace synergy::gui {

/**
 * @brief Translates a key written by Synergy 1.20 or earlier into the key and
 * value the current settings layer uses.
 *
 * Read by both the one-time settings migration and the locked settings file,
 * which administrators still write in the old key names.
 *
 * @param oldKey Key as it appears in a legacy settings file.
 * @param value Value stored against that key.
 * @return The current key and value, or nothing where the setting is obsolete.
 */
std::optional<std::pair<QString, QVariant>> mapLegacySetting(const QString &oldKey, const QVariant &value);

} // namespace synergy::gui
