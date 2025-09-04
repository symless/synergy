#!/usr/bin/env sh

main() {
  uname_out="$(uname -s)"
  case "${uname_out}" in
    Darwin*) install_darwin_deps ;;
    Linux*) install_linux_deps ;;

    *)
      echo "Unsupported OS: ${uname_out}"
      exit 1
    ;;
  esac
}

install_darwin_deps() {
  brew install googletest ninja openssl --quiet
}

install_linux_deps() {
  . /etc/os-release
  os=${ID_LIKE}
  if [ -z "$os" ]; then
    os=${ID}
  fi

  echo "Detected Linux: ${os}"
  case "${os}" in
    *debian*) install_debian_deps ;;
    *fedora*) install_fedora_deps ;;
    *suse*) install_suse_deps ;;
    *arch*) install_arch_deps ;;

    *)
      echo "Unsupported Linux: ${os}"
      exit 1
      ;;
  esac
}

install_debian_deps() {

  legacy=false
  build_libportal=false

  # On Ubuntu 24.04, libportal is available but does not support the required portals,
  # so we can build a newer one since the available libei works fine for Wayland support.
  # On older Ubuntu and Debian versions, there is no libei, so Wayland support is not possible.
  . /etc/os-release || true
  if [ "$ID" = "ubuntu" ]; then
    if [ "$VERSION_ID" = "24.04" ]; then
      build_libportal=true
    elif [ "$VERSION_ID" == "22.04" ]; then
      legacy=true
    fi
  elif [ "$ID" = "debian" ]; then
    if [ "$VERSION_ID" = "12" ]; then
      legacy=true
    fi
  fi
  
  apt-get update
  apt-get install -y \
    file \
    cmake \
    build-essential \
    ninja-build \
    xorg-dev \
    libx11-dev \
    libxtst-dev \
    libssl-dev \
    libglib2.0-dev \
    libgdk-pixbuf-2.0-dev \
    libnotify-dev \
    libxkbfile-dev \
    qt6-base-dev \
    qt6-tools-dev \
    libgtk-3-dev \
    libgtest-dev \
    libgmock-dev \
    libpugixml-dev \
    libcli11-dev

  if [ $legacy = false ]; then
    echo "Installing newer libs"
    apt-get install -y libportal-dev libei-dev
  else
    echo "Skipping newer libs"
  fi

  if [ $build_libportal = true ]; then
    echo "Installing libportal build dependencies"
    apt-get install -y \
      libportal-dev \
      python3-dbusmock \
      python3-pytest \
      valac \
      protobuf-c-compiler \
      protobuf-compiler \
      libglib2.0 \
      libgtk-3-dev \
      libprotobuf-c-dev \
      libsystemd-dev \
      libgirepository1.0-dev

    echo "Building libportal"
    ./scripts/install_deps.py --meson-no-system libportal --meson-static libportal
  fi
}

install_fedora_deps() {
  dnf install -y \
    cmake \
    make \
    ninja-build \
    gcc-c++ \
    rpm-build \
    openssl-devel \
    glib2-devel \
    gdk-pixbuf2-devel \
    libXtst-devel \
    libnotify-devel \
    libxkbfile-devel \
    qt6-qtbase-devel \
    qt6-qttools-devel \
    gtk3-devel \
    gtest-devel \
    gmock-devel \
    pugixml-devel \
    libei-devel \
    libportal-devel \
    tomlplusplus-devel \
    cli11-devel
}

install_suse_deps() {
  zypper refresh
  zypper install -y --force-resolution \
    cmake \
    make \
    ninja \
    gcc-c++ \
    rpm-build \
    libopenssl-devel \
    glib2-devel \
    gdk-pixbuf-devel \
    libXtst-devel \
    libnotify-devel \
    libxkbfile-devel \
    qt6-base-devel \
    qt6-tools-devel \
    gtk3-devel \
    googletest-devel \
    googlemock-devel \
    pugixml-devel \
    libei-devel \
    libportal-devel \
    tomlplusplus-devel \
    cli11-devel
}

install_arch_deps() {
  pacman -Syu --noconfirm \
    base-devel \
    cmake \
    ninja \
    gcc \
    openssl \
    glib2 \
    gdk-pixbuf2 \
    libxtst \
    libnotify \
    libxkbfile \
    gtest \
    pugixml \
    libei \
    libportal \
    qt6-base \
    qt6-tools \
    gtk3 \
    tomlplusplus \
    cli11
}

main "$@"
