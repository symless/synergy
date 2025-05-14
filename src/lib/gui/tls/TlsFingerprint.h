/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2015 Symless Ltd.
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

class TlsFingerprint
{
private:
  explicit TlsFingerprint(const QString &filename, bool isSystemScope);

public:
  static TlsFingerprint local(bool isSystemScope);
  static TlsFingerprint trustedServers(bool isSystemScope);
  static QString localFingerprint();
  static bool localFingerprintExists();

  void trust(const QString &fingerprintText, bool append = true) const;
  bool isTrusted(const QString &fingerprintText) const;
  QStringList readList(const int readTo = -1) const;
  QString readFirst() const;
  QString filePath() const;
  bool fileExists() const;
  QString directoryPath() const;
  void persistDirectory() const;

private:
  QString m_Filename;
  bool m_isSystemScope;
};
