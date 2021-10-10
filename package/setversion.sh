#!/bin/bash

DEFAULT_VERSION="1.2.0"
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

TAG_BUILD=0

if [ "$1" == "-tag" ]; then
    TAG_BUILD=1
    shift
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

cat package/Version.hpp.in | sed "s/#STR_VER#/$STR_VER/" | sed "s/#STR_BUILD_DATE#/$DATE/" | sed "s/#NUM_VER#/$NUM_VER/" > Common/Source/Version.hpp
cat package/AudioGridderPlugin.iss.in | sed "s/#STR_VER#/$STR_VER/" > package/AudioGridderPlugin.iss
cat package/AudioGridderServer.iss.in | sed "s/#STR_VER#/$STR_VER/" > package/AudioGridderServer.iss
cat package/archiveWin.bat.in | sed "s/#STR_VER#/$STR_VER/g" > package/archiveWin.bat

echo $STR_VER > package/VERSION
echo $NUM_VER > package/VERSION.num

if [ "$STR_VER" != "dev-build" ]; then
    #mkdir -p ../Archive/Builds/$STR_VER/win/vst
    #mkdir -p ../Archive/Builds/$STR_VER/win/vst3
    #mkdir -p ../Archive/Builds/$STR_VER/macos-x86_64
    #mkdir -p ../Archive/Builds/$STR_VER/macos-10.7-x86_64
    #mkdir -p ../Archive/Builds/$STR_VER/macos-arm64
    #mkdir -p ../Archive/Builds/$STR_VER/macos-universal
    #mkdir -p ../Archive/Builds/$STR_VER/linux

    if [ $TAG_BUILD -gt 0 ]; then
        GIT_TAG=$(echo "release_$STR_VER" | tr '[:upper:]' '[:lower:]' | sed 's/\./_/g' | sed 's/-/_/g')
        git tag $GIT_TAG
    fi
fi
