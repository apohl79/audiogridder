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
echo "AudioGridder Plugin Installer"
echo "============================="
echo
echo "Version: #version#"
echo

VST_DIR_DEFAULT="$HOME/.vst"
VST3_DIR_DEFAULT="$HOME/.vst3"

SHARE_DIR="/usr/local/share/audiogridder"

echo -n "VST Target Directory [$VST_DIR_DEFAULT]: "
read -r VST_DIR
if [ -z "$VST_DIR" ]; then
    VST_DIR=$VST_DIR_DEFAULT
fi

echo -n "VST3 Target Directory [$VST3_DIR_DEFAULT]: "
read -r VST3_DIR
if [ -z "$VST3_DIR" ]; then
    VST3_DIR=$VST3_DIR_DEFAULT
fi

echo

VST_DIR=$(echo "echo $VST_DIR" | bash)
VST3_DIR=$(echo "echo $VST3_DIR" | bash)

check_dir $VST_DIR
check_dir $VST3_DIR
check_dir $SHARE_DIR sudo

for f in ./vst/*; do
    inst $f $VST_DIR
done

for f in ./vst3/*; do
    inst $f $VST3_DIR
done

inst bin/AudioGridderPluginTray $SHARE_DIR sudo
inst bin/crashpad_handler $SHARE_DIR sudo
