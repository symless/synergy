#!/usr/bin/env python3
# Synergy -- mouse and keyboard sharing utility
# Copyright (C) 2026 Synergy App Ltd
#
# This package is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# found in the file LICENSE that should have accompanied this file.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Put a machine into the settings state an older Synergy release left behind.

Synergy carries a customer's settings forward, however old they are. An upgrade that
loses a screen layout, a server config, a TLS setting or a serial key is a support
ticket and a refund request, so the migration has to be tested against every settings
shape still in the field, which goes back years.

This is one of the real differences between Synergy and Deskflow upstream, and it comes
from who each one is for rather than from one project caring more than the other.
Deskflow's users track the newest build and rarely complain about setting something up
again when it changes, so carrying every old settings format forward would cost that
project complexity it can spend better elsewhere, and it reasonably does not. Synergy
is bought by people who may be upgrading from a version several years old and who expect
an upgrade to simply work. They are not looking to set anything up again afterwards, and
having paid for the software they have no reason to expect to. Synergy takes on the
migration code, and the work of testing it, because that expectation is part of what was
paid for.

Doing QA on it used to mean installing an old release, configuring it, and installing
the new build over the top, per release and per operating system. This writes the config
that release would have written instead, on Linux, macOS and Windows.

  legacy_config.py list                     what eras exist and whether config is saved
  legacy_config.py save                     move the live config aside
  legacy_config.py apply 1.20               write that era's config as the live config
  legacy_config.py apply 1.20 --scope system  the same, set up in the All users scope
  legacy_config.py show                     print the config as it stands now
  legacy_config.py restore                  put the saved config back
  legacy_config.py capture 1.20             save the live config as this era's fixture
  legacy_config.py clear                    remove the live config (fresh install)

Typical run:

  legacy_config.py apply 1.20      (saves the machine's own config first, on its own)
  <launch Synergy, check every setting came through>
  legacy_config.py show            (what the new build made of it)
  legacy_config.py restore

The All users scope lives under the system settings directory, so `apply --scope system`,
and `save`, `clear` and `restore` on a machine that has settings there, need administrator
rights: an elevated shell on Windows, `sudo -E` elsewhere so the user's own files are still
found under their home.

Each era carries a full config rather than a minimal one, so what is being tested is the
whole migration and not just the part that broke last time. A fixture captured from a
real install of that release is used when one exists; where none does, the era is
reconstructed from what that release's source wrote, which is a good guess and no more.
`capture` on a real install replaces the guess and the warning goes away.
"""

import argparse
import json
import os
import plistlib
import shutil
import socket
import subprocess
import sys
import tempfile
from pathlib import Path

APP = "Synergy"
IS_WINDOWS = sys.platform == "win32"
IS_MACOS = sys.platform == "darwin"
PLATFORM = "windows" if IS_WINDOWS else "macos" if IS_MACOS else "linux"

FIXTURES = Path(__file__).resolve().parent / "legacy_config"
REGISTRY_KEY = rf"Software\{APP}\{APP}"
REGISTRY_BACKUP = "registry.reg"

# 1.20 recorded the choice of the All users scope in the system file itself, and removed it
# from the user's store when the switch was made.
LEGACY_SYSTEM_SCOPE_KEY = "loadFromSystemScope"
PREFER_SYSTEM_KEY = "scope/preferSystem"

# The daemon keeps its launch command under this key on Windows and opens the settings under
# the system profile before it is handed the user's file. Both are daemon state rather than
# settings, but a fresh install has neither.
DAEMON_REGISTRY_KEY = rf"Software\{APP}"
DAEMON_REGISTRY_VALUES = ("LogLevel", "Mode", "Args", "Elevate", "Command")

# A path no user can create, for reproducing what the app does with a certificate path it cannot
# use. Only written when apply is asked for it. Windows has no unwritable root to point at, so it
# gets a reserved device name instead, which cannot be a directory on any drive.
UNUSABLE_CERT_PATH = r"C:\CON\Synergy\tls\synergy.pem" if IS_WINDOWS else "/nonexistent/Synergy/tls/synergy.pem"

# Any fixture value naming something outside the config has to be real on the machine the era is
# applied to, or the run stalls on something the migration had nothing to do with: a certificate
# directory that cannot be created, a server config that does not exist, an address that cannot be
# bound. Paths use a placeholder that resolves per platform, @USERDIR@ for the settings directory,
# @TMP@ for the temporary directory, @HOME@ and @SERVERCONFIG@ for the rest, the interface is the
# loopback, and log files go to a writable temporary directory. A path hard-coded to one platform's
# shape would still pass the launch check on the others, since a missing certificate directory is
# simply created, and the fixture would quietly stop matching what the old release wrote.
#
# An era that turns on the external server config has to leave a real file behind. The name
# is ours, so clear can remove it again without touching anything of the developer's.
SERVER_CONFIG_NAME = "legacy-config.sgc"

def server_layout(server, client):
    """The server configuration a release wrote under its one nested group.

    Every release from 1.14 to the current one has kept the screen layout and the server
    options under internalConfig in the same shape, so one set serves every era. The grid is
    5 by 3 with the server in the middle cell and the client to its left; Qt stores it as an
    array, which is why it carries a size and a one-based index per cell. This is what a
    customer has spent the most time setting up and would notice losing first.
    """
    return {
        "internalConfig/numColumns": "5",
        "internalConfig/numRows": "3",
        "internalConfig/screens/size": "15",
        "internalConfig/screens/7/name": client,
        "internalConfig/screens/8/name": server,
        "internalConfig/hotkeys/size": "0",
        "internalConfig/clipboardSharing": "false",
        "internalConfig/hasSwitchDelay": "true",
        "internalConfig/switchDelay": "500",
    }


# Settings shapes, oldest first. Each is one way a release left the machine, not one
# release: every version up to 1.20 wrote flat keys into the native store, with the server
# configuration nested under one group, so the only differences that matter are which keys
# are there and what they were called.
#
# Each era carries a whole config, the settings a customer actually set and would notice
# losing, plus a few keys that no longer exist, so a run shows both what the migration
# carries and what it drops. Auto-hide stays off in all of them, since an era that hides
# the window cannot be checked by eye.
#
# "native" is what the release wrote through QSettings' native backend (an ini file on
# Linux, a plist on macOS, the registry on Windows). "conf" and "extra" are the files the
# 1.21 line uses. "plist" is the file name the macOS native store had at the time, which
# follows the organisation domain the app was built with and therefore changes with it.
ERAS = {
    "1.14": {
        "summary": "Up to 1.14. The oldest key names still in the field.",
        "verified": False,
        "plist": "com.http-symless-com.Synergy.plist",
        "native": {
            "screenName": "legacy-1-14",
            "port": "24801",
            "interface": "127.0.0.1",
            "logLevel2": "2",
            "logToFile": "true",
            "logFilename": "@TMP@/synergy-1-14.log",
            "startedBefore": "true",
            "groupServerChecked": "true",
            "groupClientChecked": "false",
            "useExternalConfig": "true",
            "configFile": "@SERVERCONFIG@",
            "serverHostname": "localhost",
            "cryptoEnabled": "true",
            "tlsCertPath": "@USERDIR@/SSL/Synergy.pem",
            "tlsKeyLength": "2048",
            "elevateMode": "true",
            "autoHide": "false",
            "preventSleep": "true",
            "languageSync": "true",
            "invertScrollDirection": "true",
            "serialKey": "@SERIAL_KEY@",
            "activationHasRun": "true",
            "lastVersion": "1.14.5",

            # A config of this age carried both, and only the enum is mapped.
            "elevateModeEnum": "2",

            # Retired long ago, and here to prove they are dropped rather than carried.
            "edition": "2",
            "language": "en",
            "autoConfig": "false",
            "eliteBackersUrl": "https://symless.com/backers",
            **server_layout("legacy-1-14", "legacy-1-14-client"),
        },
    },
    "1.17": {
        "summary": "1.15 to 1.17. Elevate mode became an enum and several keys were retired.",
        "verified": False,
        "plist": "com.symless.Synergy.plist",
        "native": {
            "screenName": "legacy-1-17",
            "port": "24802",
            "interface": "127.0.0.1",
            "logLevel2": "1",
            "logToFile": "false",
            "logFilename": "@TMP@/synergy-1-17.log",
            "startedBefore": "true",
            "groupServerChecked": "false",
            "groupClientChecked": "true",
            "useExternalConfig": "false",
            "configFile": "@SERVERCONFIG@",
            "serverHostname": "localhost",
            "cryptoEnabled": "true",
            "tlsCertPath": "@USERDIR@/SSL/Synergy.pem",
            "tlsKeyLength": "2048",
            "elevateModeEnum": "1",
            "autoHide": "false",
            "preventSleep": "false",
            "languageSync": "true",
            "invertScrollDirection": "false",
            "closeToTray": "true",
            "showCloseReminder": "false",
            "enableUpdateCheck": "true",
            "serialKey": "@SERIAL_KEY@",
            "activated": "true",
            "lastVersion": "1.17.1",
            **server_layout("legacy-1-17", "legacy-1-17-client"),
        },
    },
    "1.20": {
        "summary": "1.18 to 1.20. What most customers upgrade from; adds service and tray keys.",
        "verified": False,
        "plist": "com.symless.Synergy.plist",
        "native": {
            "screenName": "legacy-1-20",
            "port": "24803",
            "interface": "127.0.0.1",
            "logLevel2": "3",
            "logToFile": "true",
            "logFilename": "@TMP@/synergy-1-20.log",
            "startedBefore": "true",
            "groupServerChecked": "true",
            "groupClientChecked": "false",
            "useExternalConfig": "true",
            "configFile": "@SERVERCONFIG@",
            "serverHostname": "localhost",
            "cryptoEnabled": "true",
            "tlsKeyLength": "4096",
            "autoHide": "false",
            "elevateModeEnum": "2",
            "enableService": "true",
            "closeToTray": "true",
            "showCloseReminder": "true",
            "enableUpdateCheck": "false",
            "preventSleep": "true",
            "languageSync": "false",
            "invertScrollDirection": "true",
            "serialKey": "@SERIAL_KEY@",
            "activated": "true",
            "graceStartEpochSecs": "0",
            "lastVersion": "1.20.4",

            # The certificate moved out of SSL/ into tls/ in 1.17.2.
            "tlsCertPath": "@USERDIR@/tls/synergy.pem",

            # Added in 1.20.3. Beta, because a channel that was lost comes back as stable.
            "updateTrack": "beta",
            **server_layout("legacy-1-20", "legacy-1-20-client"),
        },
    },
    "1.21-beta": {
        "summary": "The 1.21 betas. Settings already in the current layout and file.",
        "verified": False,
        "plist": None,
        "conf": {
            "core/coreMode": "1",
            "core/computerName": "legacy-1-21",
            "core/port": "24804",
            "core/preventSleep": "true",
            "core/lastVersion": "1.21.1",
            "client/remoteHost": "localhost",
            "client/languageSync": "true",
            "server/externalConfig": "true",
            "server/externalConfigFile": "@SERVERCONFIG@",
            "security/tlsEnabled": "true",
            "security/keySize": "4096",
            "gui/autoStartCore": "true",
            "gui/closeToTray": "true",
            "gui/enableUpdateCheck": "true",
            "log/level": "Debug",
        },
        "extra": {
            "license/serialKey": "@SERIAL_KEY@",
            "license/activated": "true",
            "license/holdsServerActivation": "true",
            "license/graceStartEpochSecs": "0",
            "migration/schemaVersion": "1",
            "migration/notifiedFor": "1",
        },
    },
}


def home():
    """The home of the user being tested, which under sudo is not root's."""
    sudo_user = os.environ.get("SUDO_USER")
    if sudo_user and not IS_WINDOWS and os.geteuid() == 0:
        import pwd

        return Path(pwd.getpwnam(sudo_user).pw_dir)
    return Path.home()


def own_by_user(path):
    """A file written under sudo would otherwise be root's, and the app runs as the user."""
    sudo_user = os.environ.get("SUDO_USER")
    if not sudo_user or IS_WINDOWS or os.geteuid() != 0:
        return
    try:
        if path.is_relative_to(home()):
            shutil.chown(path, user=sudo_user)
    except (OSError, LookupError):
        pass


def user_dir():
    home_dir = home()
    if IS_WINDOWS:
        return home_dir / "AppData" / "Roaming" / APP
    if IS_MACOS:
        return home_dir / "Library" / APP
    return home_dir / ".config" / APP


def conf_file():
    return user_dir() / f"{APP}.conf"


def extra_file():
    return user_dir() / f"{APP}.extra.conf"


def system_dir():
    """Where the All users scope keeps its settings, mirroring Settings::SystemDir."""
    if IS_WINDOWS:
        return Path(os.environ.get("ProgramData", r"C:\\ProgramData")) / APP
    if IS_MACOS:
        return Path("/Library") / APP
    return Path("/etc") / APP


def legacy_system_dir():
    """Where Qt put a system-scope ini file for an organisation and application both named
    after the app, which is how 1.20 opened the All users scope. Only on Windows is it the
    same directory the current release uses."""
    if IS_WINDOWS:
        return Path(os.environ.get("ProgramData", r"C:\\ProgramData")) / APP
    if IS_MACOS:
        return Path("/Library/Preferences") / APP
    return Path("/etc/xdg") / APP


def legacy_system_ini():
    return legacy_system_dir() / f"{APP}.ini"


def system_conf_file():
    return system_dir() / f"{APP}.conf"


def backup_files():
    """Where the migration leaves its backups: beside the file it migrated, or for the All
    users scope beside the user's under a name that does not replace theirs when the system
    directory could not be written."""
    return [
        user_dir() / f"{APP}.conf.legacy.bak",
        system_dir() / f"{APP}.conf.legacy.bak",
        user_dir() / f"{APP}.conf.system.legacy.bak",
    ]


def generated_files():
    """What the app writes beside its settings and a fresh install would not have: the server
    config the GUI generates for the core, and the certificate 1.20 kept at the settings root."""
    files = []
    for directory in (user_dir(), system_dir()):
        files += [directory / "synergy-server.conf", directory / f"{APP}.pem"]
    return files


def tls_dirs():
    """Certificate and fingerprint directories, the current tls/ and the SSL/ used up to 1.17."""
    return [user_dir() / "tls", user_dir() / "SSL", system_dir() / "tls", system_dir() / "SSL"]


def daemon_profile_dir():
    """The settings directory under the service account, which the daemon opens before it is
    handed the user's file."""
    if not IS_WINDOWS:
        return None
    system_root = Path(os.environ.get("SystemRoot", r"C:\\WINDOWS"))
    return system_root / "system32" / "config" / "systemprofile" / "AppData" / "Roaming" / APP


def is_writable(path):
    """Whether this process could create or replace the path, walking up to the first parent
    that exists."""
    cursor = path
    while not cursor.exists():
        if cursor.parent == cursor:
            return False
        cursor = cursor.parent
    return os.access(cursor, os.W_OK)


def locked_files():
    """Every path an administrator's locked settings file is read from."""
    paths = [system_dir() / f"{APP}.locked.ini"]
    suffix = "ini" if IS_WINDOWS else "conf"
    paths.append(legacy_system_dir() / f"{APP}.locked.{suffix}")
    return [p for p in dict.fromkeys(paths)]


def applied_scope():
    """Which scope the last apply set up, recorded so show can judge the outcome."""
    manifest_file = backup_dir() / "manifest.json"
    if not manifest_file.exists():
        return "user"
    return json.loads(manifest_file.read_text(encoding="utf-8")).get("applied_scope", "user")


def system_scope_findings(scope="user"):
    """Conditions outside this script's reach that can make a run prove the wrong thing.

    Anything that makes the app read somewhere other than the scope under test, or write over
    what the migration produced, invalidates the comparison without looking like a failure.
    """
    blocking, notes = [], []

    for path in locked_files():
        if path.exists():
            blocking.append(
                f"an administrator's locked settings file is present at {path}; it is applied "
                "after the migration and overwrites whatever the migration produced, so a "
                "setting reported as lost or changed may be its doing rather than the migration's"
            )

    if scope == "system":
        return blocking, notes

    system_conf = system_conf_file()
    if system_conf.exists():
        notes.append(
            f"the All users scope has settings at {system_conf}; this run covers the current-user "
            "scope only, and says nothing about migrating that one (apply --scope system does)"
        )

    extra = read_ini(extra_file()) or {}
    if str(extra.get(PREFER_SYSTEM_KEY, "")).lower() == "true":
        notes.append(
            "this machine prefers the All users scope; apply clears that along with the rest of "
            "the config, so the run itself is in the current-user scope, and restore puts the "
            "preference back afterwards"
        )

    return blocking, notes


def native_ini_file():
    # Only Linux keeps the native store in a file with a predictable name; it is the same
    # path as the current settings file, which is why the migration tells the two apart
    # by the keys inside rather than by the file being there.
    return Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")) / APP / f"{APP}.conf"


def plist_file(name):
    return Path.home() / "Library" / "Preferences" / name


def server_config_file():
    return user_dir() / SERVER_CONFIG_NAME


def write_server_config(screen_name):
    path = server_config_file()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "section: screens\n"
        f"\t{screen_name}:\n"
        "end\n\n"
        "section: links\n"
        "end\n\n"
        "section: options\n"
        "\tswitchCorners = none\n"
        "end\n",
        encoding="utf-8",
    )
    own_by_user(path)
    return path


def backup_dir():
    return Path.home() / ".synergy-legacy-config-backup"


def write_ini(path, values):
    """Write a QSettings ini. Keys are "group/name"; a bare name is top level.

    Only the first segment is a section. Qt keeps anything nested below it as a backslashed
    name inside that section, so "internalConfig/screens/7/name" is screens\\7\\name under
    [internalConfig], not a section of its own.
    """
    groups = {}
    for key, value in values.items():
        group, sep, name = key.partition("/")
        if not sep:
            group, name = "", key
        groups.setdefault(group, {})[name.replace("/", "\\")] = value

    lines = []
    for name, value in sorted(groups.pop("", {}).items()):
        lines.append(f"{name}={value}")
    for group in sorted(groups):
        if lines:
            lines.append("")
        lines.append(f"[{group}]")
        for name, value in sorted(groups[group].items()):
            lines.append(f"{name}={value}")

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    own_by_user(path)


def read_ini(path):
    """Read a QSettings ini back into "group/name" keys."""
    if not path.exists():
        return None
    values, group = {}, ""
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            group = line[1:-1]
            continue
        name, sep, value = line.partition("=")
        if not sep:
            continue
        key = f"{group}/{name.strip()}" if group else name.strip()
        values[key.replace("\\", "/")] = value.strip()
    return values


def write_registry(values):
    """Qt keeps a nested key in subkeys: "internalConfig/screens/7/name" is the value "name"
    under Software\\Synergy\\Synergy\\internalConfig\\screens\\7. A value written with the
    separators in its name sits at the top and the app never looks at it."""
    import winreg

    for name, value in values.items():
        path, _, leaf = name.replace("/", "\\").rpartition("\\")
        subkey = rf"{REGISTRY_KEY}\{path}" if path else REGISTRY_KEY
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, subkey) as key:
            winreg.SetValueEx(key, leaf, 0, winreg.REG_SZ, str(value))


def read_registry():
    import winreg

    def read_tree(path, prefix):
        try:
            key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, path)
        except FileNotFoundError:
            return None
        values = {}
        with key:
            index = 0
            while True:
                try:
                    name, value, _ = winreg.EnumValue(key, index)
                except OSError:
                    break
                values[f"{prefix}{name}"] = str(value)
                index += 1
            index = 0
            while True:
                try:
                    child = winreg.EnumKey(key, index)
                except OSError:
                    break
                values.update(read_tree(rf"{path}\{child}", f"{prefix}{child}/") or {})
                index += 1
        return values

    return read_tree(REGISTRY_KEY, "")


def registry_exists():
    import winreg

    try:
        winreg.OpenKey(winreg.HKEY_CURRENT_USER, REGISTRY_KEY).Close()
    except FileNotFoundError:
        return False
    return True


def export_registry(path):
    """Back the key up with reg.exe, which keeps what a rewrite by hand drops: the likes of
    port and tlsKeyLength are DWORDs that come back as strings once they have been through
    one."""
    if not registry_exists():
        return False
    result = subprocess.run(
        ["reg", "export", rf"HKCU\{REGISTRY_KEY}", str(path), "/y"], capture_output=True, check=False
    )
    return result.returncode == 0 and path.exists()


def import_registry(path):
    result = subprocess.run(["reg", "import", str(path)], capture_output=True, check=False)
    if result.returncode != 0:
        detail = result.stderr.decode(errors="replace").strip()
        raise RuntimeError(f"could not put the registry back from {path}: {detail}")


def delete_registry():
    import winreg

    def delete_tree(path):
        try:
            key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, path, 0, winreg.KEY_ALL_ACCESS)
        except FileNotFoundError:
            return
        with key:
            while True:
                try:
                    child = winreg.EnumKey(key, 0)
                except OSError:
                    break
                delete_tree(rf"{path}\{child}")
        # DeleteKey refuses a key that still holds subkeys, and fails with a permission error
        # that names nothing of the real reason, so the children have to go first.
        winreg.DeleteKey(winreg.HKEY_CURRENT_USER, path)

    delete_tree(REGISTRY_KEY)


def flush_macos_prefs():
    # cfprefsd caches preference files, so a plist written behind its back is not seen
    # until it is restarted.
    subprocess.run(["killall", "-u", os.environ.get("USER", ""), "cfprefsd"], capture_output=True, check=False)


def write_native(era, values):
    if IS_WINDOWS:
        write_registry(values)
        return f"registry {REGISTRY_KEY}"
    if IS_MACOS:
        name = era.get("plist") or f"com.symless.{APP}.plist"
        path = plist_file(name)
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("wb") as handle:
            plistlib.dump({k.replace("/", "."): v for k, v in values.items()}, handle)
        flush_macos_prefs()
        return str(path)
    path = native_ini_file()
    write_ini(path, values)
    return str(path)


def read_native(era):
    if IS_WINDOWS:
        return read_registry()
    if IS_MACOS:
        name = era.get("plist") or f"com.symless.{APP}.plist"
        path = plist_file(name)
        if not path.exists():
            return None
        with path.open("rb") as handle:
            return {k: str(v) for k, v in plistlib.load(handle).items()}
    return read_ini(native_ini_file())


def live_paths():
    """Every file or directory this script may touch, for saving and clearing.

    Both scopes are here, since a machine can hold settings in each and a run in one scope
    still has to put the other back as it found it. The system entries need administrator
    rights; save and clear say so and leave them rather than failing part way."""
    paths = [conf_file(), extra_file(), server_config_file()]
    if IS_MACOS:
        seen = {e.get("plist") for e in ERAS.values() if e.get("plist")}
        paths += [plist_file(name) for name in sorted(seen)]
    elif not IS_WINDOWS:
        paths.append(native_ini_file())
    paths += [legacy_system_ini(), system_conf_file(), *backup_files(), *generated_files(), *tls_dirs()]
    return [p for p in dict.fromkeys(paths)]


def remove_path(path):
    if path.is_dir() and not path.is_symlink():
        shutil.rmtree(path)
    else:
        path.unlink()


def clear_daemon_state(quiet):
    """A fresh Windows install has no launch command stored for the daemon and no settings
    under the service account. Both need administrator rights."""
    import winreg

    profile = daemon_profile_dir()
    if profile and profile.exists():
        try:
            shutil.rmtree(profile)
            if not quiet:
                print(f"removed {profile}")
        except PermissionError:
            print(f"warning: cannot remove {profile}; run from an elevated shell")

    try:
        key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, DAEMON_REGISTRY_KEY, 0, winreg.KEY_SET_VALUE)
    except FileNotFoundError:
        return
    except PermissionError:
        print(f"warning: cannot clear the daemon's values under HKLM\\{DAEMON_REGISTRY_KEY}; run from an elevated shell")
        return
    with key:
        removed = 0
        for name in DAEMON_REGISTRY_VALUES:
            try:
                winreg.DeleteValue(key, name)
                removed += 1
            except FileNotFoundError:
                pass
    if removed and not quiet:
        print(f"removed the daemon's values under HKLM\\{DAEMON_REGISTRY_KEY}")


def serial_key_from_test_conf():
    """Reuse the key in Synergy.test.conf so fixtures never have to carry one."""
    here = Path(__file__).resolve()
    for parent in here.parents:
        candidate = parent / f"{APP}.test.conf"
        if candidate.exists():
            values = read_ini(candidate) or {}
            return values.get("test/serialKey") or None
    return None


def cmd_list(args):
    saved = backup_dir().exists()
    print(f"platform: {PLATFORM}")
    print(f"settings: {user_dir()}")
    print(f"saved config: {'yes, ' + str(backup_dir()) if saved else 'no'}")
    print()
    for name, era in ERAS.items():
        captured = fixture_path(name).exists()
        source = "captured" if captured else "synthesised"
        print(f"  {name:<12} {source:<12} {era['summary']}")
    return 0


def fixture_path(name):
    return FIXTURES / f"{name}.{PLATFORM}.json"


def cmd_save(args):
    target = backup_dir()
    if target.exists() and not args.force:
        print(f"already saved to {target}; restore or pass --force to overwrite", file=sys.stderr)
        return 1
    if target.exists():
        shutil.rmtree(target)
    target.mkdir(parents=True)

    manifest = {"platform": PLATFORM, "files": {}}
    for index, path in enumerate(live_paths()):
        if not path.exists():
            continue
        if not is_writable(path):
            print(f"warning: cannot move {path}; run from an elevated shell to save it")
            continue
        # The two scopes share file names, so the stored name carries its position.
        stored = target / f"{index:02d}-{path.name}"
        shutil.move(str(path), stored)
        manifest["files"][str(path)] = stored.name
        print(f"saved {path}")
    if IS_WINDOWS and export_registry(target / REGISTRY_BACKUP):
        manifest["registry_file"] = REGISTRY_BACKUP
        delete_registry()
        print(f"saved registry {REGISTRY_KEY}")

    (target / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    if not manifest["files"] and "registry_file" not in manifest:
        print("nothing to save, the machine had no config")
    return 0


def cmd_restore(args):
    target = backup_dir()
    manifest_file = target / "manifest.json"
    if not manifest_file.exists():
        print(f"nothing saved at {target}", file=sys.stderr)
        return 1
    manifest = json.loads(manifest_file.read_text(encoding="utf-8"))

    # Checked before anything moves, so a run without the rights to put the system files back
    # leaves the saved config intact for a run that has them.
    unwritable = [original for original in manifest["files"] if not is_writable(Path(original))]
    if unwritable:
        for original in unwritable:
            print(f"error: cannot put back {original}", file=sys.stderr)
        print("run from an elevated shell and restore again", file=sys.stderr)
        return 1

    cmd_clear(args, quiet=True)
    for original, stored in manifest["files"].items():
        path = Path(original)
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(target / stored), path)
        own_by_user(path)
        print(f"restored {path}")
    if "registry_file" in manifest:
        import_registry(target / manifest["registry_file"])
        print(f"restored registry {REGISTRY_KEY}")
    if IS_MACOS:
        flush_macos_prefs()

    if not manifest["files"] and "registry_file" not in manifest:
        print("the saved config was empty, so the machine is back to having none")
    shutil.rmtree(target)
    return 0


def cmd_clear(args, quiet=False):
    for path in live_paths():
        if not path.exists():
            continue
        try:
            remove_path(path)
        except PermissionError:
            print(f"warning: cannot remove {path}; run from an elevated shell for a fresh install")
            continue
        if not quiet:
            print(f"removed {path}")
    if IS_WINDOWS:
        delete_registry()
        if not quiet:
            print(f"removed registry {REGISTRY_KEY}")
        clear_daemon_state(quiet)
    if IS_MACOS:
        flush_macos_prefs()
    return 0


def cmd_show(args):
    """Print the config as it stands, so a tester can see what an upgrade made of it."""
    shown = False
    stores = [
        ("native store", read_native(next(iter(ERAS.values())))),
        (str(conf_file()), read_ini(conf_file())),
        (str(extra_file()), read_ini(extra_file())),
        (str(legacy_system_ini()), read_ini(legacy_system_ini())),
        (str(system_conf_file()), read_ini(system_conf_file())),
    ]
    stores += [(str(path), read_ini(path)) for path in backup_files()]
    for label, values in stores:
        if not values:
            continue
        # On Linux the native store and the current settings file are the same file, so
        # printing both would print it twice.
        if shown and label == str(conf_file()) and conf_file() == native_ini_file():
            continue
        shown = True
        print(f"[{label}]")
        for key, value in sorted(values.items()):
            if "serialKey" in key and value:
                value = f"{value[:8]}... ({len(value)} chars)"
            print(f"  {key} = {value}")
        print()
    if not shown:
        print("no config on this machine")

    scope = applied_scope()
    blocking, notes = system_scope_findings(scope)
    if scope == "system":
        extra = read_ini(extra_file()) or {}
        if str(extra.get(PREFER_SYSTEM_KEY, "")).lower() != "true":
            notes.append(
                "the All users scope was applied but the machine now prefers the current-user "
                "scope; either the migration did not carry the preference or this launch could "
                "not write the system settings directory and fell back to the user's file"
            )
        # Qt's clear() leaves the file behind empty, so only keys still in it mean anything.
        if read_ini(legacy_system_ini()):
            notes.append(
                f"{legacy_system_ini()} still has keys in it; the migration clears it once they are "
                "carried, so either it did not run against that scope or could not write there"
            )
    for line in blocking + notes:
        print(f"note: {line}")
    return 0


def cmd_apply(args):
    args.unusable_cert_path = getattr(args, "unusable_cert_path", False)
    era = ERAS.get(args.era)
    if era is None:
        print(f"unknown era {args.era}; try list", file=sys.stderr)
        return 1
    scope = getattr(args, "scope", "user")

    blocking, notes = system_scope_findings(scope)
    for note in notes:
        print(f"note: {note}")
    if blocking and not args.force:
        for problem in blocking:
            print(f"error: {problem}", file=sys.stderr)
        print("move the file aside, or pass --force to run anyway", file=sys.stderr)
        return 1
    for problem in blocking:
        print(f"warning: {problem}")

    if scope == "system":
        for path in (legacy_system_ini(), system_conf_file()):
            if not is_writable(path):
                print(f"error: cannot write {path}; the All users scope needs an elevated shell", file=sys.stderr)
                return 1

    if not backup_dir().exists():
        print("saving the current config first")
        if cmd_save(args) != 0:
            return 1
    manifest_file = backup_dir() / "manifest.json"
    manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
    manifest["applied_scope"] = scope
    manifest_file.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    cmd_clear(args, quiet=True)

    serial_key = args.serial_key or serial_key_from_test_conf()
    captured = fixture_path(args.era)
    if captured.exists():
        data = json.loads(captured.read_text(encoding="utf-8"))
        print(f"using the fixture captured from a real {args.era} install")
    else:
        data = {k: v for k, v in era.items() if k in ("native", "conf", "extra")}
        if not era["verified"]:
            print(f"warning: no fixture for {args.era} on {PLATFORM}, synthesising from what that")
            print("         release's source wrote. Capture a real one to be sure of this test.")

    def fill(values):
        if "@SERIAL_KEY@" in json.dumps(values) and not serial_key:
            print("no serial key: pass --serial-key, or put one in Synergy.test.conf", file=sys.stderr)
            sys.exit(1)

        filled = {}
        for key, value in values.items():
            if value == "@SERIAL_KEY@":
                value = serial_key
            elif value == "@SERVERCONFIG@":
                value = str(server_config_file())
            elif isinstance(value, str):
                value = value.replace("@HOME@", str(Path.home()))
                value = value.replace("@USERDIR@", str(user_dir()))
                value = value.replace("@TMP@", tempfile.gettempdir())
            if args.unusable_cert_path and key in ("tlsCertPath", "security/certificate"):
                value = UNUSABLE_CERT_PATH
            filled[key] = value
        return filled

    screen_name = (data.get("native") or {}).get("screenName") or (data.get("conf") or {}).get(
        "core/computerName", "legacy"
    )
    # The All users copy gets its own screen name, so what comes back says which scope it
    # came from: 1.20 left the user's store in place when the switch was made, and a run has
    # to show the migration took the system one as live rather than the other.
    system_screen_name = f"{screen_name}-all-users"
    if "@SERVERCONFIG@" in json.dumps(data):
        print(f"wrote {write_server_config(system_screen_name if scope == 'system' else screen_name)}")

    if data.get("native"):
        print(f"wrote {write_native(era, fill(data['native']))}")
        if scope == "system":
            system_values = {
                key: system_screen_name if value == screen_name else value
                for key, value in fill(data["native"]).items()
            }
            system_values[LEGACY_SYSTEM_SCOPE_KEY] = "true"
            write_ini(legacy_system_ini(), system_values)
            print(f"wrote {legacy_system_ini()}")
    if data.get("conf"):
        target = system_conf_file() if scope == "system" else conf_file()
        write_ini(target, fill(data["conf"]))
        print(f"wrote {target}")
    # Only an era already on the current layout carries the scope preference. For the older
    # ones the migration has to derive it from the legacy key, which is part of what is tested.
    if data.get("extra") or (scope == "system" and data.get("conf")):
        extra = fill(data.get("extra") or {})
        if scope == "system" and data.get("conf"):
            extra[PREFER_SYSTEM_KEY] = "true"
        write_ini(extra_file(), extra)
        print(f"wrote {extra_file()}")

    written = {}
    for store in ("native", "conf", "extra"):
        if data.get(store):
            written.update(fill(data[store]))

    problems = check_launchable(written, args.unusable_cert_path)
    print()
    if problems:
        print("this config will not launch cleanly:")
        for problem in problems:
            print(f"  {problem}")
        print()
        print("fix the era rather than working around it here; a run that stalls on one of these")
        print("looks like a defect and is not one")
        return 1

    where = " set to the All users scope" if scope == "system" else ""
    print(f"the machine now looks like a {args.era} install{where}; launch Synergy to test the upgrade")
    print("run restore when done")
    return 0


def check_launchable(values, allow_unusable_cert):
    """Report anything in a written config that would stop the app launching.

    An era names things that live outside the config: a certificate directory, a server config
    file, an address to bind. If any of those is not real on this machine, the run stalls on
    something the migration had nothing to do with, which is worse than useless because it looks
    like a defect. Better to say so here than to spend a launch finding out.
    """
    problems = []

    def value_of(*keys):
        for key in keys:
            if values.get(key):
                return values[key]
        return None

    cert = value_of("tlsCertPath", "security/certificate")
    if cert and not (allow_unusable_cert and cert == UNUSABLE_CERT_PATH):
        directory = Path(cert).parent
        if not directory.exists():
            try:
                directory.mkdir(parents=True, exist_ok=True)
            except OSError as error:
                problems.append(f"certificate directory cannot be created: {directory} ({error})")

    server_config = value_of("configFile", "server/externalConfigFile")
    if server_config and not Path(server_config).exists():
        problems.append(f"server config file does not exist: {server_config}")

    log_file = value_of("logFilename", "log/file")
    if log_file and not Path(log_file).parent.exists():
        problems.append(f"log directory does not exist: {Path(log_file).parent}")

    interface = value_of("interface", "core/interface")
    port = value_of("port", "core/port")
    if interface or port:
        probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            probe.bind((interface or "", int(port or 0)))
        except OSError as error:
            problems.append(f"cannot bind {interface or 'any address'} port {port or 'any'}: {error}")
        finally:
            probe.close()

    if str(value_of("autoHide", "gui/autoHide")).lower() == "true":
        problems.append("auto-hide is on, so the window will hide itself and cannot be checked by eye")

    host = value_of("serverHostname", "client/remoteHost")
    if host:
        try:
            socket.getaddrinfo(host, None)
        except OSError:
            problems.append(f"server hostname does not resolve: {host}")

    return problems


def cmd_capture(args):
    era = ERAS.get(args.era)
    if era is None:
        print(f"unknown era {args.era}; try list", file=sys.stderr)
        return 1

    # A saved config means the live one was written by apply, and capturing that would
    # turn a guess into a fixture that claims to have come from a real install.
    if backup_dir().exists():
        print("refusing to capture: this config came from apply, not from a real install", file=sys.stderr)
        print("restore first, then capture on a machine running that release", file=sys.stderr)
        return 1

    data = {}
    native = read_native(era)
    if native:
        data["native"] = native
    # On Linux the native store is the same file as the current settings, so reading both
    # would record one file twice.
    conf = None if conf_file() == native_ini_file() and native else read_ini(conf_file())
    if conf:
        data["conf"] = conf
    extra = read_ini(extra_file())
    if extra:
        data["extra"] = extra
    if not data:
        print("found no config to capture; is this machine running that release?", file=sys.stderr)
        return 1

    redacted = 0
    for store in data.values():
        for key in store:
            if "serialKey" in key and store[key]:
                store[key] = "@SERIAL_KEY@"
                redacted += 1

    FIXTURES.mkdir(parents=True, exist_ok=True)
    path = fixture_path(args.era)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote {path}")
    if redacted:
        print(f"replaced {redacted} serial key value(s) with a placeholder; commit this file")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("list", help="show the eras and whether a config is saved")

    save = sub.add_parser("save", help="move the live config aside")
    save.add_argument("--force", action="store_true", help="overwrite an existing saved config")

    sub.add_parser("show", help="print the config as it stands now")
    sub.add_parser("restore", help="put the saved config back")
    sub.add_parser("clear", help="remove the live config, as if never installed")

    apply_cmd = sub.add_parser("apply", help="write an era's config as the live config")
    apply_cmd.add_argument("era", help="an era from list")
    apply_cmd.add_argument("--serial-key", help="key to write; defaults to the one in Synergy.test.conf")
    apply_cmd.add_argument(
        "--scope",
        choices=("user", "system"),
        default="user",
        help="which scope the era was using; system is the All users scope and needs an elevated shell",
    )
    apply_cmd.add_argument(
        "--force", action="store_true", help="overwrite an existing saved config, and run despite a locked settings file"
    )
    apply_cmd.add_argument(
        "--unusable-cert-path",
        action="store_true",
        help="point the tls certificate at a directory that cannot be created, to reproduce that error",
    )

    capture = sub.add_parser("capture", help="save the live config as this era's fixture")
    capture.add_argument("era", help="an era from list")

    args = parser.parse_args()
    handlers = {
        "list": cmd_list,
        "save": cmd_save,
        "show": cmd_show,
        "restore": cmd_restore,
        "clear": cmd_clear,
        "apply": cmd_apply,
        "capture": cmd_capture,
    }
    return handlers[args.command](args)


if __name__ == "__main__":
    sys.exit(main())
