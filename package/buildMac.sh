#!/bin/bash

xcodebuild -project Server/Builds/MacOSX/AudioGridderServer.xcodeproj -alltargets -configuration Debug clean
xcodebuild -project Server/Builds/MacOSX/AudioGridderServer.xcodeproj -alltargets -configuration Debug build
xcodebuild -project Plugin/Builds/MacOSX/AudioGridder.xcodeproj -alltargets -configuration Debug clean
xcodebuild -project Plugin/Builds/MacOSX/AudioGridder.xcodeproj -alltargets -configuration Debug build

if [ -n "$(which packagesbuild)" ]; then
    VERSION=$(cat package/VERSION)
    TARGET=package/build/AudioGridder_${VERSION}.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridder.pkgproj
    mv package/build/AudioGridder.pkg $TARGET

    echo
    echo "Created $TARGET"
fi
