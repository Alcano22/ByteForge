#!/usr/bin/env bash

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

OUT="${1:-context.txt}"
MAX_BYTES=$((256 * 1024))

mapfile -t files < <(git ls-files --cached --others --exclude-standard | sort -u)
{
    echo "=== PROJECT STRUCTURE ==="
    for file in "${files[@]}"; do
        [ -f "$file" ] && [ "$file" != "$OUT" ] && echo "$file"
    done

    echo ""
    echo "=== FILE CONTENTS ==="

    for file in "${files[@]}"; do
        [ -f "$file" ] || continue
        [ "$file" = "$OUT" ] && continue

        echo ""
        echo "--- FILE: $file ---"

        size=$(stat -c %s "$file")
        if (( size > MAX_BYTES )); then
            echo "(skipped: $((size / 1024)) KB)"
            continue
        fi

        if (( size > 0 )) && ! grep -qI '' "$file"; then
            echo "(skipped: binary file)"
            continue
        fi

        cat "$file"
    done
} > "$OUT"

echo "Written to: $OUT"
echo "Files: ${#files[@]}"
echo "Lines: $(wc -l < "$OUT")"
