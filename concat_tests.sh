#!/usr/bin/env bash

# Directory containing your test .c files
TEST_DIR="tests"

# Output file
OUTPUT="tests.txt"

# Clear or create the output file
> "$OUTPUT"

# Loop through all .c files in the tests directory
for file in "$TEST_DIR"/*.c; do
    # Extract just the filename (no path)
    fname=$(basename "$file")

    # Write header and file contents
    echo "// $fname" >> "$OUTPUT"
    cat "$file" >> "$OUTPUT"
    echo -e "\n" >> "$OUTPUT"
done

echo "Collected all test .c files into $OUTPUT"
