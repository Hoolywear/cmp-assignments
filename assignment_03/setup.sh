#!/bin/bash

# Default values and script argument parsing
dirname_arg="/usr/bin"

while getopts d: opt; do
    case $opt in
        d) dirname_arg=$OPTARG ;;
        *) echo 'error in command line parsing' >&2
           exit 1
    esac
done

shift "$(( OPTIND - 1 ))"

# standard path for MacOS
# export LLVM_DIR=/opt/homebrew/opt/llvm && echo "Exported MacOS LLVM directory"
# default option (either passing -d path/to/llvm-dir or defaulting to /usr/bin)
export LLVM_DIR=$dirname_arg && echo "Exported LLVM directory"

# Actual setup

if [ ! -d build ]; then
  mkdir build && echo "Created build directory, changing into it..."
else
  echo "Build directory already created, changing into it..."
fi
cd build

cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../

make
