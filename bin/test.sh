#!/bin/sh

. /opt/poky/3.2.1/environment-setup-cortexa57-poky-linux

CC=$CC CPP=$CXX ARCH=aarch64 make -j20 test


