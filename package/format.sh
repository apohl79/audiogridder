#!/bin/bash

DIR=.
if [ ! -d $DIR/Plugin ]; then
    DIR=..
fi
if [ -n "$1" ]; then
    DIR=$1
fi

function format() {
    if [ "$(basename $f)" != "json.hpp" ]; then
        dos2unix $f
        clang-format --verbose -i $f
    fi
}

for f in $(find $DIR/Plugin/Source -name "*.[ch]pp"); do
    format $f
done
for f in $(find $DIR/Server/Source -name "*.[ch]pp"); do
    format $f
done
