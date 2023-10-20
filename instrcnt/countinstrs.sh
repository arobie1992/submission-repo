#!/bin/bash

# exit on failure
set -e

OUTFILE="callgrind-results.txt"
FILE=$1

# valgrind seems to think unqualified files are commands instead, so start at pwd to ensure it sees it as a file
valgrind --log-file="out.log" --tool=callgrind --callgrind-out-file="$OUTFILE" ./"$FILE"

echo "
Program execution finished. Total number of executed instructions:"
callgrind_annotate --auto=yes "$OUTFILE" | grep "PROGRAM TOTALS"