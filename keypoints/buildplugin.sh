#!/bin/bash

set -e

requirements=(cmake clang-17 lldb-17 lld-17 clangd-17)
unmet=""
for r in ${requirements[@]}; do
    if [[ -z "$(which "$r")" ]]; then
       unmet="$unmet $r"
    fi
done

if [[ -n "$unmet" ]]; then
    echo "Please install the following:$unmet"
    exit 1
fi

llvm_dir=${LLVM_DIR:-"/usr/lib/llvm-17"}
if [[ ! -d "$llvm_dir" ]]; then
    echo "Unable to locate LLVM directory. 
If you did not set it, please include it by rerunning the command with LLVM_DIR set.
If you did specify it, we were unable to locate that directory."
    exit 1
fi

if [[ ! -d "$llvm_dir/lib/cmake/llvm" ]]; then
    echo "Unable to find LLVM cmake directory. Please verify your LLVM installation."
    exit 1
fi

pushd "$(dirname $0)"

rm -rf build/
mkdir build/
cd build/

LLVM_DIR="$llvm_dir" cmake ..
make

popd