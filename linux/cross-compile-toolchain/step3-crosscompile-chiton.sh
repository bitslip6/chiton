. /opt/poky/3.2.1/environment-setup-cortexa57-poky-linux
git submodule update --init --recursive
CC=$CC CPP=$CXX ARCH=aarch64 make

