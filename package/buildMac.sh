#!/bin/bash

xcodebuild -project Server/Builds/MacOSX/AudioGridderServer.xcodeproj -alltargets -configuration Release clean
xcodebuild -project Server/Builds/MacOSX/AudioGridderServer.xcodeproj -alltargets -configuration Release build
xcodebuild -project Plugin/Fx/Builds/MacOSX/AudioGridder.xcodeproj -alltargets -configuration Release clean
xcodebuild -project Plugin/Fx/Builds/MacOSX/AudioGridder.xcodeproj -alltargets -configuration Release build
xcodebuild -project Plugin/Inst/Builds/MacOSX/AudioGridderInst.xcodeproj -alltargets -configuration Release clean
xcodebuild -project Plugin/Inst/Builds/MacOSX/AudioGridderInst.xcodeproj -alltargets -configuration Release build

if [ -n "$(which packagesbuild)" ]; then
    VERSION=$(cat package/VERSION)
    TARGET=package/build/AudioGridder_${VERSION}.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridder.pkgproj
    mv package/build/AudioGridder.pkg $TARGET

    echo
    echo "Created $TARGET"
fi
