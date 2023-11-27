#!/bin/bash

# exit on failure
set -e

if [[ -z "$(which valgrind)" ]]; then
    echo "Please install valgrind. You can do this by running 'sudo apt install valgrind -y'."
    exit 1
fi

pid=$$
OUTFILE="callgrind-results.txt.${pid}"
EXECUTABLE=$1
ARGS=${@:2}

# valgrind seems to think unqualified files are commands instead, so start at pwd to ensure it sees it as a file
valgrind --log-file="out.log" --tool=callgrind --callgrind-out-file="$OUTFILE" ./"$EXECUTABLE" "$ARGS"

echo "
Program execution finished. Total number of executed instructions:"
callgrind_annotate --auto=yes "$OUTFILE" | grep "PROGRAM TOTALS"