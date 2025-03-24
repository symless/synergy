/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2012 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
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

#if !defined(DESKFLOW_VERSION)
#error version was not passed to the compiler
#endif

const auto kAppId = DESKFLOW_APP_ID;
const auto kAppName = DESKFLOW_APP_NAME;
const auto kAppDescription = "Mouse and keyboard sharing utility";
const auto kVersion = DESKFLOW_VERSION;
const auto kDaemonBinName = DESKFLOW_APP_ID "-daemon";
const auto kDaemonIpcName = DESKFLOW_APP_ID "-daemon";
const auto kDaemonLogFilename = DESKFLOW_APP_ID "-daemon.log";

#ifdef GIT_SHA_SHORT
const auto kVersionGitSha = GIT_SHA_SHORT;
const auto kDisplayVersion = DESKFLOW_VERSION " (" GIT_SHA_SHORT ")";
#else
const auto kVersionGitSha = "";
const auto kDisplayVersion = DESKFLOW_VERSION;
#endif

#ifndef NDEBUG
const auto kDebugBuild = true;
#else
const auto kDebugBuild = false;
#endif

#ifdef _WIN32

const auto kWindowsRegistryKey = "SOFTWARE\\" DESKFLOW_APP_NAME;
const auto kCloseEventName = "Global\\" DESKFLOW_APP_NAME "Close";
const auto kSendSasEventName = "Global\\" DESKFLOW_APP_NAME "SendSAS";

#endif
