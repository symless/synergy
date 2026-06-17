/*
 * synergy -- mouse and keyboard sharing utility
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

#include <chrono> // IWYU pragma: keep -- clangd wrongly thinks this is unused

namespace synergy::gui {

const auto kUrlApi = "https://symless.com/synergy/api";
const auto kUrlWebsite = QStringLiteral("https://synergyapp.io");
const auto kUrlSourceQuery = "utm_source=gui-s1";

const auto kUrlGpl = QStringLiteral("https://www.gnu.org/licenses/old-licenses/gpl-2.0.html");
const auto kUrlEula = QString("%1/eula").arg(kUrlWebsite);

const auto kLink = R"(<a href="%1" style="color: %2">%3</a>)";
const auto kLinkBuy = R"(<a href="%1" style="color: %2">Buy now</a>)";
const auto kLinkRenew = R"(<a href="%1" style="color: %2">Renew now</a>)";
const auto kLinkDownload = R"(<a href="%1" style="color: %2">Download now</a>)";

const auto kUrlPersonalUpgrade = QString("%1/purchase/upgrade?%2").arg(kUrlWebsite, kUrlSourceQuery);
const auto kUrlContact = QString("%1/contact?%2").arg(kUrlWebsite, kUrlSourceQuery);
const auto kUrlAccount = QString("%1/account?%2").arg(kUrlWebsite, kUrlSourceQuery);

constexpr auto kLicenseGracePeriod = std::chrono::days{14};
constexpr auto kRemoteCheckInterval = std::chrono::hours{24};

} // namespace synergy::gui
