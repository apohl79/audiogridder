#!/bin/bash

VERSION=$(cat package/VERSION)

function package() {
    PROJECT=$1
    PKG=$2
    TARGET=$3
    packagesbuild --package-version "$VERSION" $PROJECT
    mv $PKG $TARGET
    echo "Created $TARGET"
}

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

    cmake -B $builddir -DCMAKE_BUILD_TYPE=$buildtype -DAG_DEPS_ROOT=$HOME/audio/ag-deps-$os-$target-$arch -DCMAKE_OSX_ARCHITECTURES=$arch -DAG_MACOS_TARGET=$target
    cmake --build $builddir -j12

    if [ -n "$AG_ENABLE_SENTRY" ]; then
        echo "copying crashpad..."
        cp $HOME/audio/ag-deps-$os-$target-$arch/bin/crashpad_handler $builddir/bin
    fi

    echo "restoring toolchain..."
    sudo xcode-select -s $toolchain_bak

    if [ $dev -eq 0 ]; then

        if [ -n "$(which packagesbuild)" ]; then
            echo

            macos_target=""
            if [ "$target" == "10.7" ]; then
                macos_target="-$target"
            fi

            package package/AudioGridderPlugin$macos_target-$arch.pkgproj package/build/AudioGridderPlugin.pkg package/build/AudioGridderPlugin_${VERSION}_macOS$macos_target-$arch.pkg
            package package/AudioGridderServer$macos_target-$arch.pkgproj package/build/AudioGridderServer.pkg package/build/AudioGridderServer_${VERSION}_macOS$macos_target-$arch.pkg
        fi

        rsync -a build-$os-$target-$arch/bin/ ../Archive/Builds/$VERSION/$os$macos_target-$arch/
        rsync -a build-$os-$target-$arch/lib/ ../Archive/Builds/$VERSION/$os$macos_target-$arch/
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

    package/createUniversalBinaries.sh
    rsync -a build-macos-universal/bin/ ../Archive/Builds/$VERSION/macos-universal/
    rsync -a build-macos-universal/lib/ ../Archive/Builds/$VERSION/macos-universal/

    package package/AudioGridderPlugin-universal.pkgproj package/build/AudioGridderPlugin.pkg package/build/AudioGridderPlugin_${VERSION}_macOS-universal.pkg
    package package/AudioGridderServer-universal.pkgproj package/build/AudioGridderServer.pkg package/build/AudioGridderServer_${VERSION}_macOS-universal.pkg

    cd package/build
    zip AudioGridder_$VERSION-MacOS-Installers.zip AudioGridderPlugin_$VERSION_*.pkg AudioGridderServer_$VERSION_*.pkg
    rm AudioGridderPlugin_$VERSION_*.pkg AudioGridderServer_$VERSION_*.pkg
    cd -
fi
