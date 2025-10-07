/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Symless Ltd.
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

const auto kVersion = DESKFLOW_VERSION;

#ifdef GIT_SHA_SHORT
const auto kVersionGitSha = GIT_SHA_SHORT;
const auto kDisplayVersion = DESKFLOW_VERSION " (" GIT_SHA_SHORT ")";
#else
const auto kVersionGitSha = "";
const auto kDisplayVersion = DESKFLOW_VERSION;
#endif

#include <string>

namespace deskflow {

inline std::string version()
{
  std::string result = kVersion;
  std::string gitSha = kVersionGitSha;
  if (!gitSha.empty()) {
    result.append(" (");
    result.append(gitSha);
    result.append(")");
  }
  return result;
}

} // namespace deskflow
