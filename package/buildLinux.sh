#!/bin/bash

rm -rf build-linux-x86_64
cmake -B build-linux-x86_64 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFMPEG_ROOT=/mnt/audio/ag-deps-linux-x86_64
cmake --build build-linux-x86_64 -j6

VERSION=$(cat package/VERSION)

mkdir -p package/build/vst
mkdir -p package/build/vst3

cp build-linux-x86_64/Plugin/AudioGridderFx_artefacts/RelWithDebInfo/VST/libAudioGridder.so package/build/vst/AudioGridder.so
cp build-linux-x86_64/Plugin/AudioGridderInst_artefacts/RelWithDebInfo/VST/libAudioGridderInst.so package/build/vst/AudioGridderInst.so
cp build-linux-x86_64/Plugin/AudioGridderMidi_artefacts/RelWithDebInfo/VST/libAudioGridderMidi.so package/build/vst/AudioGridderMidi.so
cp -r build-linux-x86_64/Plugin/AudioGridderFx_artefacts/RelWithDebInfo/VST3/AudioGridder.vst3 package/build/vst3/
cp -r build-linux-x86_64/Plugin/AudioGridderInst_artefacts/RelWithDebInfo/VST3/AudioGridderInst.vst3 package/build/vst3/
cp -r build-linux-x86_64/Plugin/AudioGridderMidi_artefacts/RelWithDebInfo/VST3/AudioGridderMidi.vst3 package/build/vst3/

cp package/build/vst/* ../Archive/Builds/$VERSION/linux
cp -r package/build/vst3/* ../Archive/Builds/$VERSION/linux

cd package/build
zip -r AudioGridder_$VERSION-Linux.zip vst vst3
rm -rf vst vst3
