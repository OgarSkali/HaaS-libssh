#!/bin/sh -x

. ../Cross.sh

echo ${CROSS_PREFIX}gcc

if [ -f ${CROSS_PREFIX}gcc ]; then
	echo "ready ....."
fi

export CFLAGS="-fPIC"

export CC=${CROSS_PREFIX}gcc
export AR=${CROSS_PREFIX}ar

make clean

./configure --prefix=/usr

make

make install

