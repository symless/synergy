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

namespace synergy::gui {

// QA/test overrides for Synergy. The config file is `${kAppName}.test.conf`,
// found by walking up from the application binary's directory: a git-ignored
// copy at the repo root covers any build dir inside the tree and survives
// wipes of the app settings dir. When nothing is found above the binary
// (e.g. installed builds), falls back to `${UserDir}/${kAppName}.test.conf`,
// sibling to the main Synergy.conf settings file. Sourcing values from a file
// (rather than env vars) sidesteps the pain of getting env vars into GUI
// launches on macOS / Windows / VS Code F5.
class TestSettings
{
public:
  static TestSettings &instance();

  // True when the test config file exists and `[test]/enabled=true`. The
  // value is cached at construction; call reload() to re-read.
  bool isEnabled() const
  {
    return m_enabled;
  }

  // True when test mode is on AND `[test]/licensing=true`. Drives the
  // license activation flow when set; off by default so test mode can be
  // on (Test menu, URL overrides, etc.) without forcing license prompts.
  bool isLicensingEnabled() const
  {
    return m_enabled && m_licensing;
  }

  // Test overrides; empty / 0 unless isEnabled().
  QString serialKey() const
  {
    return m_serialKey;
  }
  QString apiUrlBase() const
  {
    return m_apiUrlBase;
  }
  QString machineId() const
  {
    return m_machineId;
  }
  qint64 startTimeEpochSecs() const
  {
    return m_startTimeEpochSecs;
  }

  // Feature toggles read from the [features] section.
  bool verbose() const
  {
    return m_verbose;
  }
  bool skipRemoteCheck() const
  {
    return m_skipRemoteCheck;
  }
  bool allowExpiredLicenses() const
  {
    return m_allowExpiredLicenses;
  }

  // Path to the test config file (whether or not it exists).
  QString fileName() const
  {
    return m_fileName;
  }

  // Re-read the file from disk (e.g., from a Test menu "Reload" action).
  void reload();

private:
  TestSettings();
  TestSettings(const TestSettings &) = delete;
  TestSettings &operator=(const TestSettings &) = delete;

  void load();

  QString m_fileName;
  bool m_enabled = false;
  bool m_licensing = false;
  QString m_serialKey;
  QString m_apiUrlBase;
  QString m_machineId;
  qint64 m_startTimeEpochSecs = 0;
  bool m_verbose = false;
  bool m_skipRemoteCheck = false;
  bool m_allowExpiredLicenses = false;
};

} // namespace synergy::gui
