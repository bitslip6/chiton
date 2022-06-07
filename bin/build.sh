#!/bin/sh
proc=$(nproc)

. /opt/poky/3.2.1/environment-setup-cortexa57-poky-linux
tmp=install
target=${1:-$tmp}


echo "compile $target with $proc threads"
CC=$CC CPP=$CXX ARCH=aarch64 make -j$proc $target


