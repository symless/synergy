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

// Style constants used by Synergy UI. Previously lived in upstream's
// `gui/styles.h`; that header was removed during the deskflow gui refactor,
// so we keep our own copy here in the overlay.

const auto kColorWhite = "#ffffff";
const auto kColorPrimary = "#ff7c00";
const auto kColorSecondary = "#4285f4";
const auto kColorNotice = "#3b67d3";

const auto kStyleNoticeLabel = //
    QString("padding: 3px 5px; border-radius: 3px;"
            "background-color: %1; color: %2")
        .arg(kColorNotice, kColorWhite);
