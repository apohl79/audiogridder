#!/bin/bash

SRC_X86=build-macos-10.8-x86_64
SRC_ARM=build-macos-11.1-arm64
TARGET=build-macos-universal

if [ ! -d $TARGET ]; then
    mkdir -p $TARGET
fi

function runlipo() {
    FILE=$1
    echo $FILE
    FILE_ARM=$(echo $FILE | sed "s,$TARGET,$SRC_ARM,")
    lipo -create $FILE $FILE_ARM -output $FILE
}

function create() {
    if [ -d $TARGET/$1 ]; then
        rm -rf $TARGET/$1
    fi
    mkdir -p $TARGET/$1
    rsync -a $SRC_X86/$1/ $TARGET/$1/
    for FILE in $(find $TARGET/$1 -type f | grep MacOS | grep -v __Pace_Eden); do
        runlipo $FILE
    done
}

sudo xcode-select -s /Library/Developer/CommandLineTools

echo "creating universal binaries..."
create bin
create lib
runlipo $TARGET/bin/crashpad_handler
