#!/bin/bash

opt_macos=false
opt_linux=false

while getopts f:o:ml opt; do
    case $opt in
        f) COMPLETE_FILEPATH=$OPTARG ;;
        o) OPT_PASS=$OPTARG ;;
        m) opt_macos=true ;;
        l) opt_linux=true ;;
        *) echo 'error while parsing arguments' >&2
           exit 1
    esac
done

shift "$(( OPTIND - 1 ))"

# For MacOS specific library extension; exported only with -m flag
"$opt_macos" && LIB_EXT=".dylib" && echo "Running script on MacOS (dylib estension)"
# same for Linux
"$opt_linux" && LIB_EXT=".so" && echo "Running script on Linux (so extension)"

if [ -z $COMPLETE_FILEPATH ]; then
    echo "No file specified (use -f PATH/TO/Test.cpp)"
    exit 1
elif [ -z $LIB_EXT ]; then
    echo "No platform specified (use -m for MacOS or -l for Linux)"
    exit 1
elif [ -z $OPT_PASS ]; then
    echo "No optimization pass specified (use -o OPT_PASS)"
    exit 1
fi

# Complete library name, to be changed for each assignment
LIB_NAME="build/libAs01Pass$LIB_EXT"

if [ -f $COMPLETE_FILEPATH ]; then
    echo "Compiling source file $COMPLETE_FILEPATH" as "${COMPLETE_FILEPATH%.*}.ll"
    clang -S -emit-llvm -O -Xclang -disable-llvm-passes $COMPLETE_FILEPATH -o "${COMPLETE_FILEPATH%.*}".ll
    opt -S -passes=mem2reg "${COMPLETE_FILEPATH%.*}".ll -o "${COMPLETE_FILEPATH%.*}".ll
else
    echo "File $COMPLETE_FILEPATH not found"
    exit 1
fi

if [ -n $OPT_PASS ]; then
    echo "Optimizing ${COMPLETE_FILEPATH%.*}.ll as ${COMPLETE_FILEPATH%.*}-opt.ll with $OPT_PASS optimization"
    opt -S -load-pass-plugin $LIB_NAME -p $OPT_PASS "${COMPLETE_FILEPATH%.*}".ll -o "${COMPLETE_FILEPATH%.*}"-opt.ll
fi
