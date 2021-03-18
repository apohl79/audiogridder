#!/bin/bash

function build() {
    os=$1
    arch=$2
    target=$3
    toolchain=$4
    dev=$5
    builddir=build-$os-$target-$arch
    buildtype=RelWithDebInfo

    if [ $dev -gt 0 ]; then
        builddir=build-dev
        buildtype=Debug
    fi

    echo "setting toolchain..."
    toolchain_bak="$(xcode-select -p)"
    sudo xcode-select -s $toolchain

    rm -rf $builddir
    cmake -B $builddir -DCMAKE_BUILD_TYPE=$buildtype -DFFMPEG_ROOT=$HOME/audio/ag-deps-$os-$target-$arch -DCMAKE_OSX_ARCHITECTURES=$arch -DAG_MACOS_TARGET=$target
    cmake --build $builddir -j12

    echo "restoring toolchain..."
    sudo xcode-select -s $toolchain_bak

    VERSION=$(cat package/VERSION)

    if [ -n "$(which packagesbuild)" ]; then
        echo

        macos_target=""
        if [ "$target" == "10.7" ]; then
            macos_target="-$target"
        fi

        TARGET=package/build/AudioGridderPlugin_${VERSION}_macOS$macos_target-$arch.pkg
        packagesbuild --package-version "$VERSION" package/AudioGridderPlugin$macos_target-$arch.pkgproj
        mv package/build/AudioGridderPlugin.pkg $TARGET
        echo "Created $TARGET"

        TARGET=package/build/AudioGridderServer_${VERSION}_macOS$macos_target-$arch.pkg
        packagesbuild --package-version "$VERSION" package/AudioGridderServer$macos_target-$arch.pkgproj
        mv package/build/AudioGridderServer.pkg $TARGET
        echo "Created $TARGET"
    fi

    if [ $dev -eq 0 ]; then
        rsync -a build-$os-$target-$arch/Server/AudioGridderServer_artefacts/RelWithDebInfo/ ../Archive/Builds/$VERSION/$os$macos_target-$arch/
        rsync -a build-$os-$target-$arch/Plugin/AudioGridderFx_artefacts/RelWithDebInfo/ ../Archive/Builds/$VERSION/$os$macos_target-$arch/
        rsync -a build-$os-$target-$arch/Plugin/AudioGridderInst_artefacts/RelWithDebInfo/ ../Archive/Builds/$VERSION/$os$macos_target-$arch/
        rsync -a build-$os-$target-$arch/Plugin/AudioGridderMidi_artefacts/RelWithDebInfo/ ../Archive/Builds/$VERSION/$os$macos_target-$arch/
    fi
}

if [ "$1" == "dev" ]; then
    echo
    echo "--- DEV BUILD ---"
    echo
    # macOS 10.8 X86_64
    build macos x86_64 10.8 /Library/Developer/10/CommandLineTools 1
else
    # macOS 10.8 X86_64
    build macos x86_64 10.8 /Library/Developer/10/CommandLineTools 0

    # macOS 10.7 X86_64
    build macos x86_64 10.7 /Library/Developer/10/CommandLineTools 0

    # macOS 11.1 ARM64
    build macos arm64 11.1 /Library/Developer/CommandLineTools 0

    cd package/build
    zip AudioGridder_$VERSION-MacOS-Installers.zip AudioGridderPlugin_$VERSION_*.pkg AudioGridderServer_$VERSION_*.pkg
    rm AudioGridderPlugin_$VERSION_*.pkg AudioGridderServer_$VERSION_*.pkg
    cd -
fi
