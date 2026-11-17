#!/usr/bin/env bash
# Counts the lines of code in ByteForge with cloc.
# Only files tracked by git are counted, so build output, install folders and vcpkg are skipped automatically.
#
# Usage: ./count_lines.sh              total per language
#        ./count_lines.sh --by-module  one table per top-level module, then the total

set -euo pipefail

cd "$(dirname "$0")"

if ! command -v cloc > /dev/null; then
    echo "cloc is not installed (Fedora: sudo dnf install cloc)" >&2
    exit 1
fi

EXCLUDED_DIRS="vendor,third_party,external"

MODULES=(Engine Editor Hub ScriptCore Player)

count() {
    cloc --vcs=git --exclude-dir="$EXCLUDED_DIRS" --quiet "$@"
}

if [[ "${1:-}" == "--by-module" ]]; then
    for module in "${MODULES[@]}"; do
        if [[ -d "$module" ]]; then
            echo "=== $module ==="
            git ls-files -- "$module" > /tmp/byteforge_cloc_files.txt
            cloc --list-file=/tmp/byteforge_cloc_files.txt --exclude-dir="$EXCLUDED_DIRS" --quiet
            echo
        fi
    done
    rm -f /tmp/byteforge_cloc_files.txt
    echo "=== Total ==="
fi

count
