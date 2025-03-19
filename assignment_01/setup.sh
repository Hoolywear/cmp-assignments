#!/bin/bash

# needed on MacOS
export LLVM_DIR=/opt/homebrew/opt/llvm

cd build

cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../

make
