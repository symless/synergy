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

// Single point of translation from cmake-defined preprocessor macros to
// typed C++ constants. Code elsewhere should consume these constants and
// never reference SYNERGY_* macros directly. Keeps the macro surface
// contained and makes the build-flavor knobs greppable.

namespace synergy {

constexpr auto kDisplayName = SYNERGY_DISPLAY_NAME;
constexpr auto kProductLine = SYNERGY_PRODUCT_LINE;

#ifdef SYNERGY_VERSION_DEV
constexpr bool kIsDevBuild = true;
#else
constexpr bool kIsDevBuild = false;
#endif

} // namespace synergy
