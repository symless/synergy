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

#include <QMap>
#include <QString>
#include <QVariant>

class QWidget;

namespace synergy::gui {

/**
 * @brief The settings an administrator has fixed for everyone on a machine,
 * read from `Synergy.locked.ini` in the system settings directory.
 *
 * Keys may be written in the names Synergy 1.20 used, which is what the help
 * centre documents and what deployments in the field contain, or in the
 * current names. Both end up keyed by the current name here.
 */
class LockedSettings
{
public:
  static LockedSettings &instance();

  /**
   * @brief Copies the locked values over the active settings, so everything
   * reading a setting afterwards sees the administrator's value.
   *
   * Runs once per launch, before the main window is built.
   */
  void apply() const;

  /**
   * @brief Whether the administrator has fixed this setting.
   * @param key Current-format settings key, e.g. `Settings::Security::TlsEnabled`.
   */
  bool isLocked(const QString &key) const;

  /**
   * @brief Disables, and explains in a tooltip, every control in a dialog
   * whose setting the administrator has fixed.
   *
   * @param dialog Settings or server configuration dialog. Controls it does
   * not contain are skipped, so either dialog can be passed.
   */
  void applyToDialog(QWidget *dialog) const;

  /// @brief Path the locked file was read from, whether or not it exists.
  QString fileName() const
  {
    return m_fileName;
  }

  /// @brief Re-reads the file from disk.
  void reload();

private:
  LockedSettings();
  LockedSettings(const LockedSettings &) = delete;
  LockedSettings &operator=(const LockedSettings &) = delete;

  void load();

  QString m_fileName;
  QMap<QString, QVariant> m_values;
};

} // namespace synergy::gui
