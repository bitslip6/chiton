#!/bin/bash

# copy yocto 3.2
git clone -b gatesgarth git://git.yoctoproject.org/poky.git
cd poky 
git checkout a7e38263628a0e8ae71ced8b6a00c1d1a9d342cd
cd ..

git clone -b gatesgarth git@github.com:openembedded/meta-openembedded.git
cd meta-openembedded
git checkout b9dcf17700014dec679307dc1247882dccd439ca
cd ..

# 
DOWNLOADS=/opt/yocto_downloads
OUTPUT_DIR=$PWD/firmware-3.2-chiton

# create download folder
sudo mkdir -pv ${DOWNLOADS}
sudo chmod 777 ${DOWNLOADS}

# create output folder
mkdir -pv ${OUTPUT_DIR}/conf

# copy configuration
cp -av chiton-configs/config-3.2/*.conf ${OUTPUT_DIR}/conf/
source poky/oe-init-build-env           ${OUTPUT_DIR}

#bitbake-layers show-layers
#bitbake core-image-full-cmdline
#bitbake package-index
#bitbake -f -c cleanall  chiton
#bitbake -f -c compile chiton


#sudo rsync -r -v -u  tmp/deploy/rpm /var/www/html/repo-3.2
MACHINE=qemuarm64 bitbake core-image-minimal -c populate_sdk
#MACHINE=qemuarm64 bitbake core-image-minimal -c populate_sdk_ext
#MACHINE=qemuarm64 runqemu core-image-full-cmdline nographic
#MACHINE=qemuarm64 runqemu core-image-full-cmdline  bootparams="console=tty0" qemuparams="-m 512"


