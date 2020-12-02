#!/bin/bash -x


source /opt/poky/3.1.3/environment-setup-aarch64-poky-linux

source /opt/fsl-imx-xwayland/4.14-sumo/environment-setup-aarch64-poky-linux
source /opt/fsl-imx-xwayland/4.14-sumo/sysroots/x86_64-pokysdk-linux/environment-setup.d/cmake.sh

export ROOT=$PWD
export BUILD=$ROOT/build_arm64
export DEPLOY=$ROOT/deploy_arm64

cd $ROOT
rm -fv release/files/app3.html
cp -av release/app3.html release/files/

rm -rf $BUILD
rm -rf $DEPLOY
mkdir -pv $BUILD
mkdir -pv $DEPLOY

function build(){
echo "####################### $1 ####################"
  mkdir -pv $BUILD/$1
  cd $BUILD/$1
  cmake -DWITH_GFLAGS=off -DWITH_NETWORK=OFF  -DWITH_EXAMPLES=off -DCMAKE_PREFIX_PATH="$DEPLOY" -DCMAKE_INSTALL_PREFIX=$DEPLOY $ROOT/$1
  make -j$(nproc)
  make install
}

build glog
build aditof_sdk
#build chiton
cd $ROOT
rm -f CMakeCache.txt
cmake -DWITH_GFLAGS=off -DWITH_NETWORK=OFF  -DWITH_EXAMPLES=off -DCMAKE_PREFIX_PATH="$DEPLOY" -DCMAKE_INSTALL_PREFIX=$DEPLOY .
make -j$(nproc)
make install



