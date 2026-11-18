#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "$0")"

for tool in rsvg-convert magick; do
    if ! command -v "$tool" >/dev/null; then
        echo "$0 needs '$tool' (Fedora: dnf install librsvg2-tools ImageMagick)" >&2
        exit 1
    fi
done

sizes=(16 24 32 48 64 128 256)
pngs=()

for size in "${sizes[@]}"; do
    mkdir -p "icons/$size"
    rsvg-convert -w "$size" -h "$size" byteforge.svg -o "icons/$size/byteforge.png"
    pngs+=("icons/$size/byteforge.png")
done

magick "${pngs[@]}" byteforge.ico

echo "Wrote ${#sizes[@]} PNGs and byteforge.ico"
