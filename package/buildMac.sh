#!/bin/bash

sudo xcode-select -s /Library/Developer/10/CommandLineTools

# macOS 10.8 X86_64
os=macos
arch=x86_64
target=10.8
rm -rf build-$os-$target-$arch
cmake -B build-$os-$target-$arch -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFMPEG_ROOT=$HOME/audio/ag-deps-$os-$arch -DCMAKE_OSX_ARCHITECTURES=$arch -DAG_MACOS_TARGET=$target
cmake --build build-$os-$target-$arch -j12

# macOS 10.7 X86_64
os=macos
arch=x86_64
target=10.7
rm -rf build-$os-$target-$arch
cmake -B build-$os-$target-$arch -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFMPEG_ROOT=$HOME/audio/ag-deps-$os-10.7-$arch -DCMAKE_OSX_ARCHITECTURES=$arch -DAG_MACOS_TARGET=$target
cmake --build build-$os-$target-$arch -j12

sudo xcode-select -s /Library/Developer/CommandLineTools
xcode-select -p

# macOS 11.1 ARM64
os=macos
arch=arm64
target=11.1
rm -rf build-$os-$target-$arch
cmake -B build-$os-$target-$arch -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFMPEG_ROOT=$HOME/audio/ag-deps-$os-$arch -DCMAKE_OSX_ARCHITECTURES=$arch -DAG_MACOS_TARGET=$target
cmake --build build-$os-$target-$arch -j8

VERSION=$(cat package/VERSION)

if [ -n "$(which packagesbuild)" ]; then
    echo

    TARGET=package/build/AudioGridderPlugin_${VERSION}_macOS-x86_64.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridderPlugin-x86_64.pkgproj
    mv package/build/AudioGridderPlugin.pkg $TARGET
    echo "Created $TARGET"

    TARGET=package/build/AudioGridderServer_${VERSION}_macOS-x86_64.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridderServer-x86_64.pkgproj
    mv package/build/AudioGridderServer.pkg $TARGET
    echo "Created $TARGET"

    TARGET=package/build/AudioGridderPlugin_${VERSION}_macOS-10.7-x86_64.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridderPlugin10.7-x86_64.pkgproj
    mv package/build/AudioGridderPlugin.pkg $TARGET
    echo "Created $TARGET"

    TARGET=package/build/AudioGridderServer_${VERSION}_macOS-10.7-x86_64.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridderServer10.7-x86_64.pkgproj
    mv package/build/AudioGridderServer10.7.pkg $TARGET
    echo "Created $TARGET"

    TARGET=package/build/AudioGridderPlugin_${VERSION}_macOS-arm64.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridderPlugin-arm64.pkgproj
    mv package/build/AudioGridderPlugin.pkg $TARGET
    echo "Created $TARGET"

    TARGET=package/build/AudioGridderServer_${VERSION}_macOS-arm64.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridderServer-arm64.pkgproj
    mv package/build/AudioGridderServer.pkg $TARGET
    echo "Created $TARGET"
fi

cp -r build-macos-10.8-x86_64/Server/AudioGridderServer_artefacts/RelWithDebInfo/AudioGridderServer.app ../Archive/Builds/$VERSION/macos-x86_64/
cp -r build-macos-11.1-arm64/Server/AudioGridderServer_artefacts/RelWithDebInfo/AudioGridderServer.app ../Archive/Builds/$VERSION/macos-arm64/
cp -r build-macos-10.7-x86_64/Server/AudioGridderServer_artefacts/RelWithDebInfo/AudioGridderServer.app ../Archive/Builds/$VERSION/macos-10.7-x86_64/
cp -r build-macos-10.8-x86_64/Plugin/AudioGridderFx_artefacts/RelWithDebInfo/* ../Archive/Builds/$VERSION/macos-x86_64/
cp -r build-macos-10.8-x86_64/Plugin/AudioGridderInst_artefacts/RelWithDebInfo/* ../Archive/Builds/$VERSION/macos-x86_64/
cp -r build-macos-10.8-x86_64/Plugin/AudioGridderMidi_artefacts/RelWithDebInfo/* ../Archive/Builds/$VERSION/macos-x86_64/
cp -r build-macos-11.1-arm64/Plugin/AudioGridderFx_artefacts/RelWithDebInfo/* ../Archive/Builds/$VERSION/macos-arm64/
cp -r build-macos-11.1-arm64/Plugin/AudioGridderInst_artefacts/RelWithDebInfo/* ../Archive/Builds/$VERSION/macos-arm64/
cp -r build-macos-11.1-arm64/Plugin/AudioGridderMidi_artefacts/RelWithDebInfo/* ../Archive/Builds/$VERSION/macos-arm64/
cp -r build-macos-10.7-x86_64/Plugin/AudioGridderFx_artefacts/RelWithDebInfo/* ../Archive/Builds/$VERSION/macos-10.7-x86_64/
cp -r build-macos-10.7-x86_64/Plugin/AudioGridderInst_artefacts/RelWithDebInfo/* ../Archive/Builds/$VERSION/macos-10.7-x86_64/
cp -r build-macos-10.7-x86_64/Plugin/AudioGridderMidi_artefacts/RelWithDebInfo/* ../Archive/Builds/$VERSION/macos-10.7-x86_64/

cd package/build
zip AudioGridder_$VERSION-MacOS-Installers.zip AudioGridderPlugin_$VERSION_*.pkg AudioGridderServer_$VERSION_*.pkg
rm AudioGridderPlugin_$VERSION_*.pkg AudioGridderServer_$VERSION_*.pkg
cd -
