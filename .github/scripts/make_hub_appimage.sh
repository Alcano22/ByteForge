#!/usr/bin/env bash

set -euo pipefail

if [ $# -ne 3 ]; then
    echo "usage: $0 <build folder> <version> <output file>" >&2
    exit 2
fi

build_dir=$(realpath "$1")
version=$2
output=$(realpath -m "$3")
source_dir=$(realpath "$(dirname "$0")/../..")
deploy_dir="$source_dir/Hub/deploy/linux"

for tool in patchelf rsvg-convert readelf; do
    if ! command -v "$tool" >/dev/null; then
        echo "$0 needs '$tool' (Fedora: dnf install patchelf librsvg2-tools binutils)" >&2
        exit 1
    fi
done

tools="${BYTEFORGE_APPIMAGE_TOOLS:-$source_dir/.appimage-tools}"
mkdir -p "$tools"

download() {
    local name=$1 url=$2
    if [ ! -x "$tools/$name" ]; then
        echo "Downloading $name"
        curl -fsSL -o "$tools/$name" "$url"
        chmod +x "$tools/$name"
    fi
}

download linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
download linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
appdir="$work/AppDir"

built="$work/ByteForge-Hub.AppImage"

cmake --install "$build_dir" --component Hub --prefix "$appdir/usr"

rsvg-convert -w 256 -h 256 "$deploy_dir/byteforge-hub.svg" -o "$work/byteforge-hub.png"

export PATH="$tools:$PATH"

export APPIMAGE_EXTRACT_AND_RUN=1

export NO_STRIP=1

export PATCHELF="$(command -v patchelf)"

export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"

export QML_SOURCES_PATHS="$source_dir/Hub/qml"

qt_plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)

wayland_plugin=""
for candidate in libqwayland.so libqwayland-generic.so; do
    if [ -f "$qt_plugins/platforms/$candidate" ]; then
        wayland_plugin=$candidate
        break
    fi
done

if [ -n "$wayland_plugin" ]; then
    echo "Bundling the Wayland platform plugin $wayland_plugin"
    export EXTRA_PLATFORM_PLUGINS="$wayland_plugin"
    export EXTRA_QT_MODULES="waylandclient"
else
    echo "warning: this Qt has no Wayland platform plugin (qt6-qtwayland), the hub will run through XWayland" >&2
fi

cd "$work"

linuxdeploy-x86_64.AppImage \
    --appdir "$appdir" \
    --executable "$appdir/usr/bin/ByteForgeHub" \
    --desktop-file "$deploy_dir/byteforge-hub.desktop" \
    --icon-file "$deploy_dir/byteforge-hub.svg" \
    --icon-file "$work/byteforge-hub.png" \
    --plugin qt

if [ -n "$wayland_plugin" ]; then
    for kind in wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client; do
        if [ -d "$qt_plugins/$kind" ] && [ ! -d "$appdir/usr/plugins/$kind" ]; then
            echo "Adding the Qt plugins $kind"
            cp -r "$qt_plugins/$kind" "$appdir/usr/plugins/"
        fi
    done
fi

echo "Removing what the hub does not use"

for style in Fusion FluentWinUI3 Imagine Material Universal Windows macOS iOS; do
    rm -rf "$appdir/usr/qml/QtQuick/Controls/$style"
    rm -rf "$appdir/usr/qml/QtQuick/Dialogs/quickimpl/qml/+$style"
done
rm -rf "$appdir/usr/qml/QtQuick/NativeStyle"

for module in VirtualKeyboard Scene2D Scene3D Pdf; do
    rm -rf "$appdir/usr/qml/QtQuick/$module"
done

rm -f "$appdir/usr/plugins/platforminputcontexts/libqtvirtualkeyboardplugin.so"

find "$appdir/usr/plugins/imageformats" -type f ! -name "libqsvg.so" -delete 2>/dev/null || true

is_elf() {
    [ "$(head -c 4 "$1" | od -An -c | tr -d ' ')" = "177ELF" ]
}

while true; do
    needed=$(find "$appdir" -type f | while read -r file; do
        if is_elf "$file"; then
            readelf -d "$file" 2>/dev/null | awk '/\(NEEDED\)/ { gsub(/[][]/, "", $5); print $5 }'
        fi
    done | sort -u)

    removed=0
    for library in "$appdir"/usr/lib/*.so*; do
        [ -f "$library" ] || continue
        name=$(basename "$library")

        if ! grep -qxF "$name" <<< "$needed"; then
            rm -f "$library"
            removed=$((removed + 1))
        fi
    done

    [ "$removed" -eq 0 ] && break
    echo "Removed $removed unused libraries"
done

cp "$work/byteforge-hub.png" "$appdir/byteforge-hub.png"

export LINUXDEPLOY_OUTPUT_VERSION="$version"
export LDAI_OUTPUT="$built"
export OUTPUT="$built"

linuxdeploy-x86_64.AppImage \
    --appdir "$appdir" \
    --output appimage

if [ ! -f "$built" ]; then
    mv "$work"/*.AppImage "$built"
fi

mkdir -p "$(dirname "$output")"
mv -f "$built" "$output"

echo "Created $output ($(du -h "$output" | cut -f1))"
