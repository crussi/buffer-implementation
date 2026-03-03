#!/usr/bin/env bash

overall_fail=0

for testbin in "$@"; do
    name=$(basename "$testbin")
    echo ""
    echo "$name"

    output=$("$testbin" 2>&1)

    summary=$(echo "$output" | grep "Tests")
    fails=$(echo "$output" | grep ":FAIL")

    if [ -n "$fails" ]; then
        echo "$fails"
        echo "$summary"
        overall_fail=1
    else
        echo "$summary"
    fi
done

exit $overall_fail