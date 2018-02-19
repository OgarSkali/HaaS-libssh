#!/bin/sh -x

. cross/Cross.sh

export CC=${CROSS_PREFIX}gcc

export CFLAGS="-I${SYSROOT}/usr/include -I${SYSROOT}/usr/local/include"
export LDFLAGS="-L${SYSROOT}/lib -L${SYSROOT}/usr/lib"
# -Wl,-rpath=${SYSROOT}/lib"

make clean

make

${CROSS_PREFIX}strip haas
