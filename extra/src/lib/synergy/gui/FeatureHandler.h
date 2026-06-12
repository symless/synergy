/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2025 - 2026 Synergy App Ltd
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

class QDialog;
class QMainWindow;

class FeatureHandler
{
public:
  static FeatureHandler &instance()
  {
    static FeatureHandler instance;
    return instance;
  }

  void handleMainWindow(QMainWindow *mainWindow);
  void handleAppStart();
  void handleSettings(QDialog *parent) const;
  void handleAbout(QDialog *parent) const;

private:
  void addTestMenu();
  void styleUpdateNotice(QMainWindow *mainWindow) const;
  void addScopeTab(QDialog *parent) const;
  void addUpdateChannelOption(QDialog *parent) const;
  void addTagline(QDialog *parent) const;
  void setAttribution(QDialog *parent) const;
  void tightenVersionRow(QDialog *parent) const;
  void addBuildDate(QDialog *parent) const;
  void addLicenseLinks(QDialog *parent) const;
  void addTrademark(QDialog *parent) const;

  QMainWindow *m_pMainWindow = nullptr;
};
