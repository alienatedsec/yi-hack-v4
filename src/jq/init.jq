#!/bin/bash

ARCHIVE=jq-1.5.tar.gz

SCRIPT_DIR=$(cd `dirname $0` && pwd)
cd $SCRIPT_DIR

rm -rf ./_install

if [ ! -f $ARCHIVE ]; then
    wget https://github.com/stedolan/jq/releases/download/jq-1.5/$ARCHIVE
fi
tar zxvf $ARCHIVE

cd jq-1.5 || exit 1

./configure --host=arm-hisiv300-linux --disable-docs
