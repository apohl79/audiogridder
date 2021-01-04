#!/bin/bash

DEFAULT_VERSION="1.0.0"
DEFAULT_VERSION_STR="dev-build"

if [ "$1" = "-h" ]; then
    echo "Usage: $0 <num version: #.#.#> <str version>"
    exit 1
fi

if [ "$1" == "-cleardate" ]; then
    DATE=""
    shift
else
    DATE=$(date)
fi

NUM_VER=$1
STR_VER=$2

if [ -z "$NUM_VER" ]; then
    NUM_VER=$DEFAULT_VERSION
fi

if [ -z "$STR_VER" ]; then
    STR_VER=$DEFAULT_VERSION_STR
fi

echo "Setting version to: $NUM_VER $STR_VER"

VERSION_NOW=$(cat package/VERSION)

if [ "$STR_VER" != "$VERSION_NOW" ]; then
    projucer=../JUCE6/Projucer.app/Contents/MacOS/Projucer

    if [ -n "$PROJUCER" ]; then
        projucer=$PROJUCER
    fi

    if [ ! -x $projucer ]; then
        echo "Projucer not found, please set the PROJUCER env variable to the Projucer binary"
    else
        $projucer --set-version $NUM_VER Server/AudioGridderServer.jucer
        $projucer --set-version $NUM_VER Plugin/Fx/AudioGridder.jucer
        $projucer --set-version $NUM_VER Plugin/Inst/AudioGridder.jucer
    fi
fi

cat package/Version.hpp.in | sed "s/#STR_VER#/$STR_VER/" | sed "s/#STR_BUILD_DATE#/$DATE/" > Common/Source/Version.hpp
cat package/AudioGridderPlugin.iss.in | sed "s/#STR_VER#/$STR_VER/" > package/AudioGridderPlugin.iss
cat package/AudioGridderServer.iss.in | sed "s/#STR_VER#/$STR_VER/" > package/AudioGridderServer.iss
cat package/archiveWin.bat.in | sed "s/#STR_VER#/$STR_VER/g" > package/archiveWin.bat

echo $STR_VER > package/VERSION
echo $NUM_VER > package/VERSION.num

if [ "$STR_VER" != "dev-build" ]; then
    mkdir -p ../Archive/Builds/$STR_VER/win/vst
    mkdir -p ../Archive/Builds/$STR_VER/win/vst3
    mkdir -p ../Archive/Builds/$STR_VER/macos-x86_64
    mkdir -p ../Archive/Builds/$STR_VER/macos-10.7-x86_64
    mkdir -p ../Archive/Builds/$STR_VER/macos-arm64
    mkdir -p ../Archive/Builds/$STR_VER/linux
    sudo chmod -R g+rw ../Archive/Builds
fi
