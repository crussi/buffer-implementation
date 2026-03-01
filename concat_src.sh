#!/usr/bin/env bash

# Always run relative to script location (important)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"

# Output file in project root
OUT_FILE="$SCRIPT_DIR/output.txt"

# Overwrite output file if it exists
> "$OUT_FILE"

# Process all .h and .c files in alphabetical order
find "$SRC_DIR" -maxdepth 1 -type f \( -name "*.h" -o -name "*.c" \) | sort | while read -r file
do
    filename=$(basename "$file")

    # Write filename header
    echo "//$filename" >> "$OUT_FILE"

    # Blank line
    echo >> "$OUT_FILE"

    # Write file contents
    cat "$file" >> "$OUT_FILE"

    # Blank line after content
    echo >> "$OUT_FILE"
    echo >> "$OUT_FILE"

done

echo "Done. Output written to $OUT_FILE"