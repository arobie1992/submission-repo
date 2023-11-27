#!/bin/bash

EXECUTABLE_NAME="test-exe"

PLUGIN_LOCATION=$1
INPUT_FILES=${@:2}

pid=$$
tmpdir="tmp-$pid"

mkdir "$tmpdir"

cd "$tmpdir"

ALTERED_PATH_FILES=""
for f in ${INPUT_FILES[@]}; do
    ALTERED_PATH_FILES="$ALTERED_PATH_FILES ../$f"
done

clang-15 -gdwarf-4 -O0 -fpass-plugin="../$PLUGIN_LOCATION" ${ALTERED_PATH_FILES[@]}
exit_code=$?
if [[ "$exit_code" != 0 ]]; then
    echo "Compilation failed: $exit_code"
    exit 1
fi

cd ..

cp "$tmpdir"/branch_dictionary.txt ./branch_dictionary.txt."$pid"
cp "$tmpdir"/a.out ./a.out

rm -rf "$tmpdir"
