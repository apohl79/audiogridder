#!/bin/bash

SRC_X86=build-macos-10.8-x86_64
SRC_ARM=build-macos-11.1-arm64
TARGET=build-macos-universal

if [ ! -d $TARGET ]; then
    mkdir -p $TARGET
fi

function create() {
    mkdir -p $TARGET/$1
    rsync -a $SRC_X86/$1/ $TARGET/$1/
    for FILE in $(find $TARGET/$1 -type f | grep MacOS); do
        echo $FILE
        FILE_ARM=$(echo $FILE | sed "s,$TARGET,$SRC_ARM,")
        lipo -create $FILE $FILE_ARM -output $FILE
    done
}

sudo xcode-select -s /Library/Developer/CommandLineTools

echo "creating universal binaries..."
create Server/AudioGridderServer_artefacts/RelWithDebInfo/AudioGridderServer.app
create PluginTray/AudioGridderPluginTray_artefacts/RelWithDebInfo/AudioGridderPluginTray.app
create Plugin/AudioGridderFx_artefacts/RelWithDebInfo/AU
create Plugin/AudioGridderFx_artefacts/RelWithDebInfo/VST
create Plugin/AudioGridderFx_artefacts/RelWithDebInfo/VST3
create Plugin/AudioGridderInst_artefacts/RelWithDebInfo/AU
create Plugin/AudioGridderInst_artefacts/RelWithDebInfo/VST
create Plugin/AudioGridderInst_artefacts/RelWithDebInfo/VST3
create Plugin/AudioGridderMidi_artefacts/RelWithDebInfo/AU
create Plugin/AudioGridderMidi_artefacts/RelWithDebInfo/VST
create Plugin/AudioGridderMidi_artefacts/RelWithDebInfo/VST3
