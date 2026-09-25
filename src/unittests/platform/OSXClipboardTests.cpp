/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2011 Nick Bolton
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXClipboardTests.h"

#include "platform/OSXClipboard.h"
#include "platform/OSXClipboardPNGConverter.h"
#include "platform/OSXClipboardUTF8Converter.h"

#include <QDataStream>
#include <QtEndian>

#include <cstdlib>

namespace {

std::string twoPixelDib()
{
  QByteArray dib;
  QDataStream stream(&dib, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream << quint32(40) << qint32(2) << qint32(1) << quint16(1) << quint16(32) << quint32(0) << quint32(8) << qint32(0)
         << qint32(0) << quint32(0) << quint32(0);
  stream << quint32(0xFFFF0000) << quint32(0xFF0000FF);
  return dib.toStdString();
}

} // namespace

void OSXClipboardTests::open()
{
  OSXClipboard clipboard;
  QVERIFY(clipboard.open(0));
  QVERIFY(clipboard.empty());
  clipboard.close();
}

void OSXClipboardTests::singleFormat()
{
  using enum IClipboard::Format;

  OSXClipboard clipboard;
  QVERIFY(clipboard.empty());
  clipboard.add(Text, m_testString);
  QVERIFY(clipboard.has(Text));
  QCOMPARE(clipboard.get(Text), m_testString);
}

void OSXClipboardTests::formatConvert_UTF8()
{
  OSXClipboardUTF8Converter converter;
  QCOMPARE(IClipboard::Format::Text, converter.getFormat());
  QCOMPARE(converter.getOSXFormat(), CFSTR("public.utf8-plain-text"));
  QCOMPARE(converter.fromIClipboard("test data\n"), "test data\r");
  QCOMPARE(converter.toIClipboard("test data\r"), "test data\n");
}

void OSXClipboardTests::formatConvertPng()
{
  OSXClipboardPNGConverter converter;
  QCOMPARE(converter.getFormat(), IClipboard::Format::Bitmap);
  QCOMPARE(converter.getOSXFormat(), CFSTR("public.png"));

  const auto png = converter.fromIClipboard(twoPixelDib());
  QVERIFY(png.starts_with("\x89PNG"));

  const auto dib = converter.toIClipboard(png);
  QVERIFY(dib.size() >= 40);
  QCOMPARE(qFromLittleEndian<qint32>(dib.data() + 4), 2);
  QCOMPARE(std::abs(qFromLittleEndian<qint32>(dib.data() + 8)), 1);
}

void OSXClipboardTests::bitmapOfferedAsPng()
{
  OSXClipboard clipboard;
  QVERIFY(clipboard.empty());
  clipboard.add(IClipboard::Format::Bitmap, twoPixelDib());

  PasteboardRef pasteboard = nullptr;
  QVERIFY(PasteboardCreate(kPasteboardClipboard, &pasteboard) == noErr);
  PasteboardSynchronize(pasteboard);
  PasteboardItemID item = nullptr;
  QVERIFY(PasteboardGetItemIdentifier(pasteboard, 1, &item) == noErr);
  PasteboardFlavorFlags flags = 0;
  QVERIFY(PasteboardGetItemFlavorFlags(pasteboard, item, CFSTR("public.png"), &flags) == noErr);
  QVERIFY(PasteboardGetItemFlavorFlags(pasteboard, item, CFSTR("com.microsoft.bmp"), &flags) == noErr);
  CFRelease(pasteboard);
}

QTEST_MAIN(OSXClipboardTests)
