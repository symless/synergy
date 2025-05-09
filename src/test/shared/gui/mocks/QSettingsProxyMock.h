/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2025 Symless Ltd.
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

#include "gui/proxy/QSettingsProxy.h"

#include "gmock/gmock.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class QSettingsProxyMock : public deskflow::gui::proxy::QSettingsProxy
{
public:
  MOCK_METHOD(void, loadSystem, (), (override));
  MOCK_METHOD(void, loadUser, (), (override));
  MOCK_METHOD(void, loadLocked, (), (override));
  MOCK_METHOD(void, copyFrom, (const QSettingsProxy &, bool), (override));
  MOCK_METHOD(bool, fileExists, (), (const, override));
  MOCK_METHOD(QString, fileName, (), (const, override));
  MOCK_METHOD(void, sync, (), (override));
  MOCK_METHOD(bool, isWritable, (), (const, override));
  MOCK_METHOD(bool, contains, (const QString &), (const, override));
  MOCK_METHOD(QVariant, value, (const QString &), (const, override));
  MOCK_METHOD(QVariant, value, (const QString &, const QVariant &), (const, override));
  MOCK_METHOD(void, setValue, (const QString &, const QVariant &), (override));
  MOCK_METHOD(int, beginReadArray, (const QString &prefix), (override));
  MOCK_METHOD(void, beginWriteArray, (const QString &prefix), (override));
  MOCK_METHOD(void, setArrayIndex, (int i), (override));
  MOCK_METHOD(void, endArray, (), (override));
  MOCK_METHOD(void, beginGroup, (const QString &prefix), (override));
  MOCK_METHOD(void, endGroup, (), (override));
  MOCK_METHOD(void, remove, (const QString &key), (override));
};
