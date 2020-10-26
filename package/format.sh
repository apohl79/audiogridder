#!/bin/bash

function format() {
    if [ "$(basename $f)" != "json.hpp" ]; then
        dos2unix $f
        clang-format --verbose -i $f
    fi
}

if [ -n "$1" ] && [ -f $1 ]; then
    f=$1
    format $f
    exit
fi

DIR=.
if [ ! -d $DIR/Plugin ]; then
    DIR=..
fi
if [ -n "$1" ]; then
    DIR=$1
fi

for f in $(find $DIR/Plugin/Source -name "*.[ch]pp"); do
    format $f
done
for f in $(find $DIR/Server/Source -name "*.[ch]pp"); do
    format $f
done
for f in $(find $DIR/Common/Source -name "*.[ch]pp"); do
    format $f
done
