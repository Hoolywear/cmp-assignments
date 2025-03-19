#!/bin/bash

# Default values and script argument parsing
opt_macos=false
opt_linux=false

while getopts ml opt; do
    case $opt in
        m) opt_macos=true ;;
        l) opt_linux=true ;;
        *) echo 'error in command line parsing' >&2
           exit 1
    esac
done

shift "$(( OPTIND - 1 ))"

# needed on MacOS; exported only with -m flag
"$opt_macos" && export LLVM_DIR=/opt/homebrew/opt/llvm && echo "Exported MacOS LLVM directory"
# same for Linux
"$opt_linux" && export LLVM_DIR=/usr/bin && echo "Exported Linux LLVM directory"

# Actual setup

if [ ! -d build ]; then
  mkdir build && echo "Created build directory, changing into it..."
else
  echo "Build directory already created, changing into it..."
fi
cd build

cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../

make
