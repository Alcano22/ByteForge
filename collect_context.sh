#!/bin/bash

OUT="context.txt"
> "$OUT"

PRUNE_EXPR=(
    \( -path "*/build*" \
       -o -path "*/cmake-build-*" \
       -o -path "*/_deps*" \
       -o -path "*/.git*" \
       -o -path "*/.idea*" \
       -o -path "*/vendor*" \
    \) -prune -o
)

echo "=== PROJECT STRUCTURE ===" >> "$OUT"
find . "${PRUNE_EXPR[@]}" -print \
  | grep -vE '\.(lock)$' \
  | sed 's|^\./||' \
  | sort >> "$OUT"

echo "" >> "$OUT"
echo "=== FILE CONTENTS ===" >> "$OUT"

find . "${PRUNE_EXPR[@]}" -type f -print \
  | grep -E '\.(hpp|h|cpp|cmake)$|CMakeLists\.txt$' \
  | sort \
  | while read -r file; do
      echo "" >> "$OUT"
      echo "--- FILE: ${file#./} ---" >> "$OUT"
      cat "$file" >> "$OUT"
    done

echo "Written to: $OUT"
echo "Lines: $(wc -l < "$OUT")"
