#!/bin/bash
if [ -x /usr/bin/tccutil ]; then
    /usr/bin/tccutil reset All com.e47.AudioGridderServer || exit 0
fi
