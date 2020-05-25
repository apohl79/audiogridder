#!/bin/bash

NUM_VER=$1
STR_VER=$2

if [ -z "$NUM_VER" ] || [ -z "$STR_VER" ]; then
    echo "usage: $0 <num version: #.#.#> <str version>"
    exit 1
fi

projucer=../JUCE/Projucer.app/Contents/MacOS/Projucer

if [ -n "$PROJUCER" ]; then
    projucer=$PROJUCER
fi

if [ ! -x $projucer ]; then
    echo "Projucer not found, please set the PROJUCER env variable to the Projucer binary"
    exit 1
fi

$projucer --set-version $NUM_VER Server/AudioGridderServer.jucer
$projucer --set-version $NUM_VER Plugin/AudioGridder.jucer

cat package/Version.hpp.in | sed "s/#STR_VER#/$STR_VER/" > Server/Source/Version.hpp
cat package/AudioGridder.iss.in | sed "s/#STR_VER#/$STR_VER/" > package/AudioGridder.iss

echo $STR_VER > package/VERSION
