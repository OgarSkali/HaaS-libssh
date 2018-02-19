#!/bin/sh -x

. ../Cross.sh

echo ${CROSS_PREFIX}gcc

if [ -f ${CROSS_PREFIX}gcc ]; then
	echo "ready ....."
fi

export SHARED_CGLAGS="-fPIC"
export CFLAGS="-fPIC"

#export INSTALL_PREFIX=usr

make distclean

./Configure shared linux-generic32 --prefix=${SYSROOT}/usr --openssldir=${SYSROOT}/usr/openssl no-async 
# --crosscompile=${CROSS_COMPILE}

make depend

make

make install

