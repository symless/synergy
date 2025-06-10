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
import lib.meson as meson_utils


def main():
    args = parse_args()
    run(args)


def parse_args():
    parser = argparse.ArgumentParser()
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
    env.ensure_in_venv(__file__, create_venv=True)
    env.install_requirements()
    install(args)


def install(args):
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


if __name__ == "__main__":
    main()
