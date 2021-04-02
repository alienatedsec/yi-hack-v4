#!/bin/bash

export PATH=${PATH}:/opt/arm-hisiv300-linux/bin

export TARGET=arm-hisiv300-linux-uclibcgnueabi
export CROSS=arm-hisiv300-linux-uclibcgnueabi
export BUILD=x86_64-pc-linux-gnu

export CROSSPREFIX=${CROSS}-

export STRIP=${CROSSPREFIX}strip
export CXX=${CROSSPREFIX}g++
export CC=${CROSSPREFIX}gcc
export LD=${CROSSPREFIX}ld
export AS=${CROSSPREFIX}as
export AR=${CROSSPREFIX}ar

SCRIPT_DIR=$(cd `dirname $0` && pwd)
cd $SCRIPT_DIR

cd jq-1.6/modules/oniguruma
autoreconf -fi || exit 1

cd ../../../jq-1.6 || exit 1

make clean
make || exit 1

mkdir -p ../_install/bin || exit 1

cp ./jq ../_install/bin || exit 1

${STRIP} ../_install/bin/* || exit 1
