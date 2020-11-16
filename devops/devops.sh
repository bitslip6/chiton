#!/bin/bash -x

if [ $# -eq 1 ]; then

case $1 in

  ubuntu1604_intel)
    echo "Ubuntu 16.04 quirks"
    apt-get install -y libjpeg-dev libi2c-dev libinput-dev
    ;;
  ubuntu1804_intel)
    echo "Ubuntu 18.04 quirks"
    apt-get install -y libjpeg-dev libi2c-dev libinput-dev
    ;;
  ubuntu2004_intel)
    echo "Ubuntu 20.04 quirks"
    apt-get install -y libjpeg-dev libi2c-dev libinput-dev
    ;;
  ubuntu1604_yocto25_arm64)
    echo "Ubuntu 16.04  Yocto 2.5 SDK"
    source /opt/fsl-imx-xwayland/4.14-sumo/environment-setup-aarch64-poky-linux
    source /opt/fsl-imx-xwayland/4.14-sumo/sysroots/x86_64-pokysdk-linux/environment-setup.d/cmake.sh
    ;;
  ubuntu1804_yocto25_arm64)
    echo "Ubuntu 18.04  Yocto 2.5 SDK Extensible"
    source /home/sdk_sumo/poky_sdk/environment-setup-aarch64-poky-linux
#    devtool sdk-install -s libjpeg-turbo
#    devtool sdk-install -s i2c-tools
#    devtool sdk-install -s libinput

    ;;
  ubuntu1804_yocto31_arm64)
    echo "Ubuntu 18.04  Yocto 3.1 SDK Extensible"
    source /home/sdk_dunfell/poky_sdk/environment-setup-aarch64-poky-linux
#    devtool sdk-install -s libjpeg-turbo
#    devtool sdk-install -s i2c-tools
#    devtool sdk-install -s libinput
    ;;
  ubuntu2004_yocto31_arm64)
    echo "Ubuntu 20.04  Yocto 3.1 SDK"
    source /opt/poky/3.1.3/environment-setup-aarch64-poky-linux
    ;;

  *)
    echo "unknown variant"
    exit 1
    ;;
esac


function build_all(){
    export ROOT=$PWD
    #rm -rf build_*
    mkdir -pv $ROOT/build_deploy_$1
    mkdir -pv $ROOT/build_glog_$1
    mkdir -pv $ROOT/build_aditof_$1
    mkdir -pv $ROOT/build_chiton_$1
    cd $ROOT/build_glog_$1 &&
    cmake -DWITH_GFLAGS=OFF -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=$ROOT/build_deploy_$1 $ROOT/source_glog &&
    make -j$(nproc) &&
    make install &&
    cd $ROOT/build_aditof_$1 &&
    cmake --version &&
    cmake -DWITH_NETWORK=OFF -DWITH_EXAMPLES=OFF -Dglog_DIR=$ROOT/build_deploy_$1/lib/cmake/glog -DCMAKE_INSTALL_PREFIX=$ROOT/build_deploy_$1 $ROOT/source_aditof &&
    make -j$(nproc) &&
    make install &&
    cd $ROOT/build_chiton_$1 &&
    cmake -Dglog_DIR=$ROOT/build_deploy_$1/lib/cmake/glog -Daditof_DIR=$ROOT/build_deploy_$1/lib/cmake/aditof -DCMAKE_INSTALL_PREFIX=$ROOT/build_deploy_$1 $ROOT &&
    #make -j$(nproc) &&
    make &&
    make install &&
    cd $ROOT
}

git clone --branch v0.3.5 --depth 1 https://github.com/google/glog.git source_glog
git clone --branch master --depth 1 https://github.com/analogdevicesinc/aditof_sdk.git source_aditof
build_all $@

else
 echo "specific variant"
fi
