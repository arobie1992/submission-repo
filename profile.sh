#!/bin/bash

set -e

EXECUTABLE_NAME="test-exe"

INPUT_FILES=$@

pid=$$
tmpdir="tmp-$pid"

pushd $(dirname $0)

mkdir "$tmpdir"

cd "$tmpdir"

ALTERED_PATH_FILES=""
for f in ${INPUT_FILES[@]}; do
    ALTERED_PATH_FILES="$ALTERED_PATH_FILES ../$f"
done

clang-17 -gdwarf-4 -O0 -fpass-plugin=`echo ../keypoints/build/keypoints/KeyPointsPass.*` ${ALTERED_PATH_FILES[@]} ../keypoints/support/branchlog.c -o "$EXECUTABLE_NAME"

../instrcnt/countinstrs.sh "$EXECUTABLE_NAME"

cd ..

cp "$tmpdir"/branch_dictionary.txt ./branch_dictionary.txt."$pid"
if [[ -f "$tmpdir"/branch_trace.txt ]]; then
    cp "$tmpdir"/branch_trace.txt ./branch_trace.txt."$pid"
else
    touch ./branch_trace.txt."$pid"
fi

rm -rf "$tmpdir"

popd