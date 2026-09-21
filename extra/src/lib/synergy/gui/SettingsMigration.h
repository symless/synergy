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

class QWidget;

namespace synergy::gui::migration {

/// Bumped each time a new migration is added, and to re-run one that was broken in a release
/// already in customers' hands: schema 1 shipped in the 1.21 betas reading the wrong macOS
/// preferences domain, so those machines recorded a migration that carried nothing.
constexpr int kCurrentSchemaVersion = 2;

/**
 * @brief Ports legacy-format settings (Synergy 1.x, both user and system
 * scope, native QSettings backend) into the new Settings layout.
 *
 * Must run before Settings::instance() is constructed: upstream's
 * cleanSettings() strips any key not in its allow-list, which would erase
 * the legacy keys before this can read them. Idempotent, gated on
 * migration/schemaVersion in Synergy.extra.conf.
 *
 * @return true if a migration was performed this launch.
 */
bool migrateIfNeeded();

/**
 * @brief Adds a status bar pill telling the customer their settings were migrated, which
 * opens the full notice when clicked. Never a dialog of its own: startup raises several,
 * and two at once leave the window unusable.
 *
 * No-op unless a migration has run and not yet been acknowledged. Marks
 * migration/notifiedFor=schemaVersion once the customer has seen it, so a later migration
 * (bumped schema version) shows the pill again.
 */
void showNoticeIfPending(QWidget *parent);

} // namespace synergy::gui::migration
