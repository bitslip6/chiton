#!/bin/sh


tmp=$(pwd)
# GLOG from scratch
# 
git clone https://github.com/google/glog
cd glog
git checkout refs/tags/v0.3.5
autoconf
./configure
automake --add-missing
make
sudo make install
sudo ldconfig


# PROTOBUF from scratch
#
cd $tmp
git clone https://github.com/protocolbuffers/protobuf
cd protobuf/
git checkout v3.9.0
./autogen.sh 
./configure 
make -j4
sudo make -j4 install
sudo ldconfig


# LIBWEBSOCKETS from scratch
#
cd $tmp
git clone https://github.com/warmcat/libwebsockets
cd libwebsockets/
git checkout v3.1-stable
mkdir build
cd build/
cmake ..
make -j4 
sudo make -j4 install
sudo ldconfig


# some dependenmcies we may not have yet
#
sudo apt install libraspberrypi0 libraspberrypi-dev libavcodec-extra58


# aditof from scratch...
cd $tmp
git clone https://github.com/analogdevicesinc/aditof_sdk
cd aditof_sdk/
mkdir build
cd build/
cmake -DWITH_PYTHON=0 -DWITH_OPENCV=0 -DWITH_REVB=1 -DRASPBERRYPI=1 ..
sudo cmake --build . --target install
cd examples
cd aditof-demo
sudo ./aditof-demo
