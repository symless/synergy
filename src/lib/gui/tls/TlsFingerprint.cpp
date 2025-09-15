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

#include "TlsFingerprint.h"

#include "gui/paths.h"

#include <QDir>
#include <QTextStream>

static const char kDirName[] = "SSL/Fingerprints";
static const char kLocalFilename[] = "Local.txt";
static const char kTrustedServersFilename[] = "TrustedServers.txt";
static const char kTrustedClientsFilename[] = "TrustedClients.txt";

using namespace deskflow::gui;

TlsFingerprint::TlsFingerprint(const QString &filename, bool isSystemScope)
    : m_Filename(filename),
      m_isSystemScope(isSystemScope)
{
}

void TlsFingerprint::trust(const QString &fingerprintText, bool append) const
{
  TlsFingerprint::persistDirectory();

  QIODevice::OpenMode openMode;
  if (append) {
    openMode = QIODevice::Append;
  } else {
    openMode = QIODevice::WriteOnly;
  }

  QFile file(filePath());
  if (file.open(openMode)) {
    QTextStream out(&file);
    out << fingerprintText << "\n";
    file.close();
  }
}

bool TlsFingerprint::fileExists() const
{
  QString dirName = directoryPath();
  if (!QDir(dirName).exists()) {
    return false;
  }

  QFile file(filePath());
  return file.exists();
}

bool TlsFingerprint::isTrusted(const QString &fingerprintText) const
{
  QStringList list = readList();
  foreach (QString trusted, list) {
    if (trusted == fingerprintText) {
      return true;
    }
  }
  return false;
}

QStringList TlsFingerprint::readList(const int readTo) const
{
  QStringList list;

  QString dirName = directoryPath();
  if (!QDir(dirName).exists()) {
    return list;
  }

  QFile file(filePath());

  if (file.open(QIODevice::ReadOnly)) {
    QTextStream in(&file);
    while (!in.atEnd()) {
      list.append(in.readLine());
      if (list.size() == readTo) {
        break;
      }
    }
    file.close();
  }

  return list;
}

QString TlsFingerprint::readFirst() const
{
  QStringList list = readList(1);
  return list.at(0);
}

QString TlsFingerprint::filePath() const
{
  QString dir = directoryPath();
  return QString("%1/%2").arg(dir).arg(m_Filename);
}

void TlsFingerprint::persistDirectory() const
{
  QDir dir(directoryPath());
  if (!dir.exists()) {
    if (!dir.mkpath(".")) {
      qFatal("failed to create fingerprint dir: %s", qPrintable(dir.absolutePath()));
    }
  }
}

QString TlsFingerprint::directoryPath() const
{
  QDir dir = m_isSystemScope ? paths::systemConfigDir() : paths::userConfigDir();
  if (!dir.exists()) {
    qFatal("config dir does not exist: %s", qPrintable(dir.absolutePath()));
  }
  return dir.filePath(kDirName);
}

TlsFingerprint TlsFingerprint::local(bool isSystemScope)
{
  return TlsFingerprint(kLocalFilename, isSystemScope);
}

TlsFingerprint TlsFingerprint::trustedServers(bool isSystemScope)
{
  return TlsFingerprint(kTrustedServersFilename, isSystemScope);
}
