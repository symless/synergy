/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXClipboardPNGConverter.h"

#include "base/Log.h"

#include <ImageIO/ImageIO.h>

namespace {

std::string convertImage(const std::string &image, CFStringRef toType)
{
  CFDataRef sourceData = CFDataCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(image.data()), static_cast<CFIndex>(image.size())
  );
  if (sourceData == nullptr) {
    return {};
  }

  CGImageSourceRef source = CGImageSourceCreateWithData(sourceData, nullptr);
  CFRelease(sourceData);
  if (source == nullptr) {
    return {};
  }

  CGImageRef decoded = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
  CFRelease(source);
  if (decoded == nullptr) {
    return {};
  }

  std::string converted;
  CFMutableDataRef convertedData = CFDataCreateMutable(kCFAllocatorDefault, 0);
  CGImageDestinationRef destination =
      convertedData != nullptr ? CGImageDestinationCreateWithData(convertedData, toType, 1, nullptr) : nullptr;
  if (destination != nullptr) {
    CGImageDestinationAddImage(destination, decoded, nullptr);
    if (CGImageDestinationFinalize(destination)) {
      converted.assign(
          reinterpret_cast<const char *>(CFDataGetBytePtr(convertedData)),
          static_cast<size_t>(CFDataGetLength(convertedData))
      );
    }
    CFRelease(destination);
  }
  if (convertedData != nullptr) {
    CFRelease(convertedData);
  }
  CGImageRelease(decoded);

  return converted;
}

} // namespace

IClipboard::Format OSXClipboardPNGConverter::getFormat() const
{
  return IClipboard::Format::Bitmap;
}

CFStringRef OSXClipboardPNGConverter::getOSXFormat() const
{
  return CFSTR("public.png");
}

std::string OSXClipboardPNGConverter::fromIClipboard(const std::string &dib) const
{
  const auto bmp = m_bmpConverter.fromIClipboard(dib);
  if (bmp.empty()) {
    return {};
  }

  auto png = convertImage(bmp, getOSXFormat());
  if (png.empty()) {
    LOG_WARN("failed to convert clipboard image to png");
    return {};
  }

  LOG_DEBUG("converted clipboard image to png: %zu bytes", png.size());
  return png;
}

std::string OSXClipboardPNGConverter::toIClipboard(const std::string &png) const
{
  const auto bmp = convertImage(png, m_bmpConverter.getOSXFormat());
  if (bmp.empty()) {
    LOG_WARN("failed to convert clipboard image from png");
    return {};
  }

  return m_bmpConverter.toIClipboard(bmp);
}
