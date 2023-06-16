#!/bin/bash

function check_dir() {
    d=$1
    sudo=$2
    if [ ! -d $d ]; then
        $sudo mkdir -p $d
    fi
}

function inst() {
    f=$1
    t=$2
    sudo=$3
    TARGET=$t/$(basename $f)
    echo "installing $TARGET"
    if [ -d $f ]; then
        if [ -d "$t/$TARGET" ]; then
            $sudo rm -rf "$t/$TARGET"
        fi
        $sudo cp -r $f $t/
    else
        $sudo install $f $TARGET
    fi
}

echo
echo "AudioGridder Server Installer"
echo "============================="
echo
echo "Version: #version#"
echo

BIN_DIR="/usr/local/bin"
SHARE_DIR="/usr/local/share/audiogridder"
APPS_DIR="/usr/local/share/applications"

check_dir $BIN_DIR sudo
check_dir $SHARE_DIR sudo
check_dir $APPS_DIR sudo

inst bin/AudioGridderServer $BIN_DIR sudo
inst bin/crashpad_handler $SHARE_DIR sudo
inst resources/icon64.png $SHARE_DIR sudo
inst resources/audiogridderserver.desktop $APPS_DIR sudo
