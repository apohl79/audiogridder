#!/bin/bash

export CONFIG=Release

cd Plugin/Fx/Builds/LinuxMakefile
make clean
make -j4
cd -
cd Plugin/Inst/Builds/LinuxMakefile
make clean
make -j4
cd -


VERSION=$(cat package/VERSION)

mkdir -p package/build/vst
mkdir -p package/build/vst3

cp Plugin/Fx/Builds/LinuxMakefile/build/AudioGridder.so package/build/vst/
cp Plugin/Inst/Builds/LinuxMakefile/build/AudioGridderInst.so package/build/vst/
cp -r Plugin/Fx/Builds/LinuxMakefile/build/AudioGridder.vst3 package/build/vst3/
cp -r Plugin/Inst/Builds/LinuxMakefile/build/AudioGridderInst.vst3 package/build/vst3/

cp package/build/vst/* ../Archive/Builds/$VERSION/linux
cp -r package/build/vst3/* ../Archive/Builds/$VERSION/linux

cd package/build
zip -r AudioGridderPlugin_$VERSION-linux.zip vst vst3
rm -rf vst vst3
