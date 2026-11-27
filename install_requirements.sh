#!/usr/bin/env bash

set -euo pipefail

if ! command -v dnf > /dev/null; then
    echo "error: this script needs dnf (Fedora). Install the packages listed in it by hand." >&2
    exit 1
fi

packages=(
    git gcc-c++ clang make cmake ninja-build patch perl pkgconf-pkg-config
    autoconf autoconf-archive automake libtool bison flex gperf python3
    zip unzip tar curl which diffutils findutils file kernel-headers
    libXinerama-devel libXcursor-devel libXrandr-devel libXi-devel mesa-libGLU-devel
    wayland-devel libxkbcommon-devel wayland-protocols-devel systemd-devel
    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland
    dotnet-sdk-10.0 libicu
    patchelf librsvg2-tools
)

sudo=()
if [[ $EUID -ne 0 ]]; then
    sudo=(sudo)
fi

"${sudo[@]}" dnf install -y --setopt=install_weak_deps=False "${packages[@]}"

if [[ -z "${VCPKG_ROOT:-}" ]]; then
    echo
    echo "note: VCPKG_ROOT is not set. Clone and bootstrap vcpkg, then export VCPKG_ROOT:"
    echo "    git clone https://github.com/microsoft/vcpkg ~/dev/cpp/vcpkg"
    echo "    ~/dev/cpp/vcpkg/bootstrap-vcpkg.sh -disableMetrics"
    echo "    echo 'export VCPKG_ROOT=\$HOME/dev/cpp/vcpkg' >> ~/.bashrc"
fi

echo
echo "Done. Configure with: cmake --preset release"
