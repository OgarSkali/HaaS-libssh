#!/bin/sh -x

. ../../Cross.sh

export CMAKE_SYSTEM_NAME=Linux

export CMAKE_C_COMPILER=${CROSS_PREFIX}gcc
export CMAKE_CXX_COMPILER=${CROSS_PREFIX}c++

export CMAKE_FIND_ROOT_PATH=${SYSROOT}

export CFLAGS=-fPIC
export LDFLAGS=-L${SYSROOT}/usr/lib

make clean

cmake .. -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_C_COMPILER=${CROSS_PREFIX}gcc -DCMAKE_CXX_COMPILER=${CROSS_PREFIX}c++ \
			 -DCMAKE_FIND_ROOT_PATH=${SYSROOT} -DCMAKE_INSTALL_PREFIX=/usr

make

make install

