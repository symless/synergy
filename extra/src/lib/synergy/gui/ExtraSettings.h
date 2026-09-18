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

// Synergy license state (serial key, activation). Stored as
// synergy/* keys in the upstream Settings file via the new static Settings API.
class ExtraSettings
{
public:
  enum class ActivatedMode
  {
    kNone,
    kServer,
    kClient
  };

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

  /// @brief The role this machine last activated in, or none if it has never activated.
  ActivatedMode activatedMode() const
  {
    return m_activatedMode;
  }
  void setActivatedMode(ActivatedMode mode)
  {
    m_activatedMode = mode;
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
  ActivatedMode m_activatedMode = ActivatedMode::kNone;
  QString m_offlineActivationResponse;
};

} // namespace synergy::gui
