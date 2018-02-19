#!/bin/sh -x

CROSS_ROOT=/home/ogar/work/OpenWRT
CROSS_TOOL1=OpenWrt-Toolchain-brcm47xx-mips74k_gcc-5.3.0_musl-1.1.16.Linux-x86_64
CROSS_TOOL2=toolchain-mipsel_74kc+dsp2_gcc-5.3.0_musl-1.1.16
CROSS_DIR=${CROSS_ROOT}/${CROSS_TOOL1}/${CROSS_TOOL2}

#CROSS_PREFIX=
export CROSS_PREFIX=${CROSS_DIR}/bin/mipsel-openwrt-linux-musl-

export STAGING_DIR=${CROSS_ROOT}/sysroot
export SYSROOT=${STAGING_DIR}
export DESTDIR=${STAGING_DIR}

export CROSS_COMPILE=${CROSS_PREFIX}

export CC=gcc
export AR=ar



