#!/usr/bin/env python3

# Synergy -- mouse and keyboard sharing utility
# Copyright (C) 2024 Symless Ltd.
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

import os, sys, argparse, traceback
import lib.env as env
import lib.cmd_utils as cmd_utils
import lib.github as github
import lib.meson as meson_utils

path_env_var = "PATH"
cmake_prefix_env_var = "CMAKE_PREFIX_PATH"


def main():
    args = parse_args()
    run(args)


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--subprojects",
        action="store_true",
        help="Install dependencies for Meson subprojects (use with --meson-no-system)",
    )
    parser.add_argument(
        "--meson-no-system",
        nargs="+",
        help="Specify which Meson subprojects to use instead of system dependencies",
    )
    parser.add_argument(
        "--meson-static",
        nargs="+",
        help="Specify which Meson subprojects to build as static libraries",
    )

    return parser.parse_args()


def run(args):
    env.ensure_dependencies()
    env.ensure_in_venv(__file__, auto_create=True)
    env.install_requirements()
    install(args)


def install(args):
    if args.subprojects:
        for subproject in args.meson_no_system or []:
            deps = SubprojectDependencies(subproject)
            deps.install()

    run_meson(args.meson_no_system, args.meson_static, "build")


# It's a bit weird to use Meson just for installing deps, but it's a stopgap until
# we fully switch from CMake to Meson. For the meantime, Meson will install the deps
# so that CMake can find them easily. Once we switch to Meson, it might be possible for
# Meson handle the deps resolution, so that we won't need to install them on the system.
def run_meson(no_system_list, static_list, build_dir):
    meson = meson_utils.Meson(build_dir)
    meson.setup(no_system_list, static_list)

    # Only compile and install on Linux for now, since we're only using Meson to fetch
    # the deps on Windows and macOS.
    if env.is_linux():
        meson.compile()

    meson.install()


class SubprojectDependencies:

    def __init__(self, subproject):
        from lib.config import Config

        self.subproject = subproject
        self.config = Config()

    def install(self):
        """Installs dependencies for the current platform."""

        print(f"Installing dependencies for sub-project: {self.subproject}")

        if env.is_linux():
            self.linux()
        else:
            raise RuntimeError(f"Unsupported platform: {os}")

    def linux(self):
        """Installs dependencies on Linux."""
        import lib.linux as linux

        command = self.config.get_subproject_deps_command(self.subproject)
        linux.run_command(command, check=True)


if __name__ == "__main__":
    main()
