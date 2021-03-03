#!/bin/bash

sudo mkdir /opt/yocto_downloads
sudo chmod 777 /opt/yocto_downloads/

# python2 venv
virtualenv venv 
source venv/bin/activate

# remove bogus classes
rm -fv sources/meta-freescale/classes/image_types_fsl.bbclass

mkdir -pv firmware-2.5-chiton/conf
cp -av sources/meta-chiton/config-2.5/*.conf firmware-2.5-chiton/conf/
source sources/poky/oe-init-build-env firmware-2.5-chiton

bitbake-layers show-layers
DISTRO=fsl-imx-xwayland MACHINE=imx8mm-var-dart bitbake core-image-full-cmdline
bitbake package-index
sudo rsync -r -v -u  tmp/deploy/rpm /var/www/html/repo-2.5

#MACHINE=imx8mm-var-dart bitbake core-image-minimal -c populate_sdk
#MACHINE=imx8mm-var-dart bitbake core-image-minimal -c populate_sdk_ext

