#!/bin/bash

xcodebuild -project Server/Builds/MacOSX/AudioGridderServer.xcodeproj -alltargets -configuration Release clean
xcodebuild -project Server/Builds/MacOSX/AudioGridderServer.xcodeproj -alltargets -configuration Release build
xcodebuild -project Plugin/Fx/Builds/MacOSX/AudioGridder.xcodeproj -alltargets -configuration Release clean
xcodebuild -project Plugin/Fx/Builds/MacOSX/AudioGridder.xcodeproj -alltargets -configuration Release build
xcodebuild -project Plugin/Inst/Builds/MacOSX/AudioGridderInst.xcodeproj -alltargets -configuration Release clean
xcodebuild -project Plugin/Inst/Builds/MacOSX/AudioGridderInst.xcodeproj -alltargets -configuration Release build

VERSION=$(cat package/VERSION)

if [ -n "$(which packagesbuild)" ]; then
    TARGET=package/build/AudioGridder_${VERSION}.pkg
    packagesbuild -v --package-version "$VERSION" package/AudioGridder.pkgproj
    mv package/build/AudioGridder.pkg $TARGET

    echo
    echo "Created $TARGET"
fi

cp -r Server/Builds/MacOSX/build/Release/AudioGridderServer.app ../Archive/Builds/$VERSION/osx/
cp -r Plugin/Fx/Builds/MacOSX/build/Release/AudioGridder.* ../Archive/Builds/$VERSION/osx/
cp -r Plugin/Inst/Builds/MacOSX/build/Release/AudioGridderInst.* ../Archive/Builds/$VERSION/osx/
