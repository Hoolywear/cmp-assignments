#!/bin/bash

DIRPATH="test/"
COMPLETE_FILEPATH=$DIRPATH

algid_opt=false
strred_opt=false
multiinst_opt=false

while getopts f:123 opt; do
    case $opt in
        f) FILEPATH+=$OPTARG ;;
        1) algid_opt=true ;;
        2) strred_opt=true ;;
        3) multiinst_opt=true ;;
        *) echo 'error while parsing arguments' >&2
           exit 1
    esac
done

shift "$(( OPTIND - 1 ))"

if [ -z $FILEPATH ]; then
    echo "No file specified"
    exit 1
else
    COMPLETE_FILEPATH+=$FILEPATH
fi

if [ -f $COMPLETE_FILEPATH ]; then
    echo "Compiling source file $COMPLETE_FILEPATH" as "${COMPLETE_FILEPATH%.*}.ll"
    clang -S -emit-llvm -O -Xclang -disable-llvm-passes $COMPLETE_FILEPATH -o "${COMPLETE_FILEPATH%.*}".ll
    opt -S -passes=mem2reg "${COMPLETE_FILEPATH%.*}".ll -o "${COMPLETE_FILEPATH%.*}".ll
else
    echo "File $COMPLETE_FILEPATH not found"
    exit 1
fi

if [ $algid_opt = true ]; then
    echo "Optimizing ${COMPLETE_FILEPATH%.*}.ll as ${COMPLETE_FILEPATH%.*}-opt.ll with Algebraic identity optimization"
    opt -S -load-pass-plugin build/libAs01Pass.dylib -p AlgId "${COMPLETE_FILEPATH%.*}".ll -o "${COMPLETE_FILEPATH%.*}"-opt.ll
elif [ $strred_opt = true ]; then
    echo "Optimizing ${COMPLETE_FILEPATH%.*}.ll as ${COMPLETE_FILEPATH%.*}-opt.ll with Strength reduction optimization"
    opt -S -load-pass-plugin build/libAs01Pass.dylib -p AdvStrRed "${COMPLETE_FILEPATH%.*}".ll -o "${COMPLETE_FILEPATH%.*}"-opt.ll
elif [ $multiinst_opt = true ]; then
    echo "Optimizing ${COMPLETE_FILEPATH%.*}.ll as ${COMPLETE_FILEPATH%.*}-opt.ll with Multi-instruction optimization"
    opt -S -load-pass-plugin build/libAs01Pass.dylib -p MultiInstOpt "${COMPLETE_FILEPATH%.*}".ll -o "${COMPLETE_FILEPATH%.*}"-opt.ll
fi
