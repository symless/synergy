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

#pragma once

#include <QString>

namespace synergy::gui {

// Synergy license state (serial key, activation, grace period). Stored as
// synergy/* keys in the upstream Settings file via the new static Settings API.
class ExtraSettings
{
public:
  ExtraSettings() = default;

  void load();
  void sync();

  QString serialKey() const
  {
    return m_serialKey;
  }
  void setSerialKey(const QString &serialKey)
  {
    m_serialKey = serialKey;
  }

  bool activated() const
  {
    return m_activated;
  }
  void setActivated(bool activated)
  {
    m_activated = activated;
  }

  bool holdsServerActivation() const
  {
    return m_holdsServerActivation;
  }
  void setHoldsServerActivation(bool holdsServerActivation)
  {
    m_holdsServerActivation = holdsServerActivation;
  }

  qint64 graceStartEpochSecs() const
  {
    return m_graceStartEpochSecs;
  }
  void setGraceStartEpochSecs(qint64 epochSecs)
  {
    m_graceStartEpochSecs = epochSecs;
  }

  QString offlineActivationResponse() const
  {
    return m_offlineActivationResponse;
  }
  void setOfflineActivationResponse(const QString &response)
  {
    m_offlineActivationResponse = response;
  }

  QString fileName() const;
  bool isWritable() const;

private:
  QString m_serialKey;
  bool m_activated = false;
  bool m_holdsServerActivation = false;
  qint64 m_graceStartEpochSecs = 0;
  QString m_offlineActivationResponse;
};

} // namespace synergy::gui
