#!/bin/bash

# copy yocto 3.2
git clone -b gatesgarth git://git.yoctoproject.org/poky.git

git clone -b gatesgarth git@github.com:openembedded/meta-openembedded.git

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
bitbake core-image-full-cmdline
#bitbake package-index

#sudo rsync -r -v -u  tmp/deploy/rpm /var/www/html/repo-3.2
#MACHINE=qemuarm64 bitbake core-image-minimal -c populate_sdk
#MACHINE=qemuarm64 bitbake core-image-minimal -c populate_sdk_ext
MACHINE=qemuarm64 runqemu core-image-full-cmdline nographic


