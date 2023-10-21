#!/bin/bash

set -e

EXECUTABLE_NAME="test-exe"

FILE=$1

pid=$$
tmpdir="tmp-$pid"

pushd $(dirname $0)

mkdir "$tmpdir"

cd "$tmpdir"

clang-17 -gdwarf-4 -fpass-plugin=`echo ../keypoints/build/keypoints/KeyPointsPass.*` ../"$FILE" ../keypoints/support/branchlog.c -o "$EXECUTABLE_NAME"

../instrcnt/countinstrs.sh "$EXECUTABLE_NAME"

cd ..

cp "$tmpdir"/branch_dictionary.txt ./branch_dictionary.txt."$pid"
cp "$tmpdir"/branch_trace.txt ./branch_trace.txt."$pid"

rm -rf "$tmpdir"

popd