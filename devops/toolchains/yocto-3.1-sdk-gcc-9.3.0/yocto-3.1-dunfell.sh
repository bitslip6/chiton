
mkdir -pv sources
cd sources
git clone -b dunfell git://git.yoctoproject.org/poky.git
git clone -b dunfell https://github.com/Freescale/meta-freescale.git
git clone -b dunfell https://github.com/Freescale/meta-freescale-3rdparty.git
git clone -b dunfell https://github.com/varigit/meta-variscite-fslc.git

cd ..
source sources/poky/oe-init-build-env
bitbake core-image-minimal -c populate_sdk


