#!/bin/bash

set -e

EXECUTABLE_NAME="test-exe"

FILE=$1

pushd $(dirname $0)

# build the plugin
# see what Dr. Shen says about this
# keypoints/buildplugin.sh > /dev/null

clang-17 -g -fpass-plugin=`echo keypoints/build/keypoints/KeyPointsPass.*` "$FILE" keypoints/keypoints/branchlog.c -o "$EXECUTABLE_NAME"

instrcnt/countinstrs.sh "$EXECUTABLE_NAME"

popd