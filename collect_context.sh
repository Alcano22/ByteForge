#!/bin/bash

OUT="context.txt"
> "$OUT"

PRUNE_EXPR=(
    \( -path "*/build*" \
       -o -path "*/cmake-build-*" \
       -o -path "*/_deps*" \
       -o -path "*/.git*" \
       -o -path "*/.idea*" \
       -o -path "*/.vscode*" \
       -o -path "*/vendor*" \
       -o -path "*/bin" \
       -o -path "*/obj" \
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
  | grep -E '\.(hpp|h|cpp|cmake|cs|csproj|sln|props|targets)$|CMakeLists\.txt$' \
  | sort \
  | while read -r file; do
      echo "" >> "$OUT"
      echo "--- FILE: ${file#./} ---" >> "$OUT"
      cat "$file" >> "$OUT"
    done

echo "Written to: $OUT"
echo "Lines: $(wc -l < "$OUT")"
