#!/bin/bash

PROJECT=all
PRETTIFY=0

case "$1" in
    "-help")
        echo "Usage: $0 [-server|-plugins|-fx|-inst|-midi|-compiledb] [-clean] [-keeptc]"
        exit
        ;;
    "-server")
        PROJECT=server
        shift
        ;;
    "-plugins")
        PROJECT=plugins
        shift
        ;;
    "-fx")
        PROJECT=fx
        shift
        ;;
    "-inst")
        PROJECT=inst
        shift
        ;;
    "-midi")
        PROJECT=midi
        shift
        ;;
    "-compiledb")
        PROJECT=compiledb
        PRETTIFY=1
        shift
        ;;
esac

if [ "$1" == "-clean" ]; then
    CLEAN=y
    shift
fi

UPDATE_TOOLCHAIN=1
if [ "$1" == "-keeptc" ]; then
    UPDATE_TOOLCHAIN=0
    shift
fi

TOOLCHAIN=/Applications/Xcode10.3.app/Contents/Developer
LAST_TC=$(xcode-select -p)

if [ $UPDATE_TOOLCHAIN -gt 0 ]; then
    echo "setting toolchain to $TOOLCHAIN"
    sudo xcode-select -s $TOOLCHAIN
fi

rm -rf xcodebuild.log

if [ -z "$CONFIG" ]; then
    CONFIG=Debug
fi

COMPDBENABLED=1
COMPDBFILE=""
XCPROJECT=""
COMPDBPARAM="-r json-compilation-database"

function xc() {
    if [ $PRETTIFY -gt 0 ]; then
        xcodebuild -project $1 -arch x86_64 -alltargets -configuration $2 $3 | $4
    else
        esc=$(printf '\033')
        xcodebuild -project $1 -arch x86_64 -alltargets -configuration $2 $3 \
            | egrep --line-buffered -v "^[{}]?$|ARCHS|Prepare|Clean\.Remove|CopyPlistFile|CpResource|Touch|Entitlements|Create |Rez " \
            | egrep --line-buffered -v "Build|Write|ProcessInfoPlistFile|RegisterWithLaunchServices|ProcessProductPackaging|StripNIB " \
            | egrep --line-buffered -v "clang -x|clang\+\+ |export |cd |mkdir |builtin-|note|Signing |security\.get-task-allow|touch " \
            | egrep --line-buffered -v "ResMergerCollector |ResMergerProduct |codesign |Check |SymLink " \
            | sed -l -E "s,^(CompileC).*\.o (.*) normal.*$,${esc}[32m\1${esc}[0m \2," \
            | sed -l -E "s,^(Ld|Libtool) (.*) normal.*$,${esc}[32m\1${esc}[0m \2," \
            | sed -l -E "s,^(CodeSign) (.*)$,${esc}[32m\1${esc}[0m \2,"
    fi
}

function build() {
    PRETTYCMD=""
    PRETTYCMD_WITH_COMPDB=$PRETTYCMD
    if [ $PRETTIFY -gt 0 ]; then
        PRETTYCMD="xcpretty"
        PRETTYCMD_WITH_COMPDB=$PRETTYCMD
    fi
    if [ "$CONFIG" == "Release" ] || [ -n "$CLEAN" ]; then
        xc $XCPROJECT $CONFIG clean $PRETTYCMD
    else
        COMPDBENABLED=0
    fi
    if [ $PRETTIFY -gt 0 ] && [ $COMPDBENABLED -gt 0 ]; then
        PRETTYCMD_WITH_COMPDB="$PRETTYCMD $COMPDBPARAM"
    fi
    xc $XCPROJECT $CONFIG build $PRETTYCMD_WITH_COMPDB
    if [ -n "$COMPDBFILE" ] && [ -e build/reports/compilation_db.json ]; then
        mv build/reports/compilation_db.json $COMPDBFILE
    fi
}

if [ "$PROJECT" == "all" ] || [ "$PROJECT" == "server" ] || [ "$PROJECT" == "compiledb" ]; then
    XCPROJECT=Server/Builds/MacOSX/AudioGridderServer.xcodeproj
    COMPDBENABLED=1
    COMPDBFILE=Server/compile_commands.json
    build
fi
if [ "$PROJECT" == "all" ] || [ "$PROJECT" == "server10.7" ]; then
    XCPROJECT=Server/Builds/MacOSX10.7/AudioGridderServer.xcodeproj
    COMPDBENABLED=0
    COMPDBFILE=""
    build
fi
if [ "$PROJECT" == "all" ] || [ "$PROJECT" == "fx" ] || [ "$PROJECT" == "plugins" ] || [ "$PROJECT" == "compiledb" ]; then
    XCPROJECT=Plugin/Fx/Builds/MacOSX/AudioGridder.xcodeproj
    COMPDBENABLED=1
    COMPDBFILE=Plugin/compile_commands.json
    build
fi
if [ "$PROJECT" == "all" ] || [ "$PROJECT" == "inst" ] || [ "$PROJECT" == "plugins" ]; then
    XCPROJECT=Plugin/Inst/Builds/MacOSX/AudioGridderInst.xcodeproj
    COMPDBENABLED=0
    COMPDBFILE=""
    build
fi
if [ "$PROJECT" == "all" ] || [ "$PROJECT" == "midi" ] || [ "$PROJECT" == "plugins" ]; then
    XCPROJECT=Plugin/Midi/Builds/MacOSX/AudioGridderMidi.xcodeproj
    COMPDBENABLED=0
    COMPDBFILE=""
    build
fi

if [ $UPDATE_TOOLCHAIN -gt 0 ]; then
    echo "setting toolchain back to $LAST_TC"
    sudo xcode-select -s $LAST_TC
fi

if [ -e Plugin/compile_commands.json ] && [ -e Server/compile_commands.json ]; then
    echo "merging compile commands"
    echo "[" > compile_commands.json
    cat Plugin/compile_commands.json | json_pp | egrep -v '^\[|\]$' >> compile_commands.json
    echo "," >> compile_commands.json
    cat Server/compile_commands.json | json_pp | egrep -v '^\[|\]$' >> compile_commands.json
    echo "]" >> compile_commands.json
    rm Plugin/compile_commands.json Server/compile_commands.json
fi

if [ "$CONFIG" == "Debug" ]; then
    exit
fi

VERSION=$(cat package/VERSION)

if [ -n "$(which packagesbuild)" ]; then
    echo

    TARGET=package/build/AudioGridderPlugin_${VERSION}.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridderPlugin.pkgproj
    mv package/build/AudioGridderPlugin.pkg $TARGET
    echo "Created $TARGET"

    TARGET=package/build/AudioGridderServer_${VERSION}.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridderServer.pkgproj
    mv package/build/AudioGridderServer.pkg $TARGET
    echo "Created $TARGET"

    TARGET=package/build/AudioGridderServer10.7_${VERSION}.pkg
    packagesbuild --package-version "$VERSION" package/AudioGridderServer10.7.pkgproj
    mv package/build/AudioGridderServer10.7.pkg $TARGET
    echo "Created $TARGET"
fi

cp -r Server/Builds/MacOSX/build/Release/AudioGridderServer.app ../Archive/Builds/$VERSION/osx/
cp -r Server/Builds/MacOSX10.7/build/Release/AudioGridderServer.app ../Archive/Builds/$VERSION/osx10.7/
cp -r Plugin/Fx/Builds/MacOSX/build/Release/AudioGridder.* ../Archive/Builds/$VERSION/osx/
cp -r Plugin/Inst/Builds/MacOSX/build/Release/AudioGridderInst.* ../Archive/Builds/$VERSION/osx/
cp -r Plugin/Midi/Builds/MacOSX/build/Release/AudioGridderMidi.* ../Archive/Builds/$VERSION/osx/

cd package/build
zip AudioGridder_$VERSION-osx.zip AudioGridderPlugin_$VERSION.pkg AudioGridderServer10.7_$VERSION.pkg AudioGridderServer_$VERSION.pkg
rm AudioGridderPlugin_$VERSION.pkg AudioGridderServer10.7_$VERSION.pkg AudioGridderServer_$VERSION.pkg
cd -
