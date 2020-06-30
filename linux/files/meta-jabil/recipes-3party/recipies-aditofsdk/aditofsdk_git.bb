# Recipe created by recipetool
# This is the basis of a recipe and may need further editing in order to be fully functional.
# (Feel free to remove these comments when editing.)

# WARNING: the following LICENSE and LIC_FILES_CHKSUM values are best guesses - it is
# your responsibility to verify that the values are complete and correct.
#
# The following license files were not able to be identified and are
# represented as "Unknown" below, you will need to check them yourself:
#   LICENSE
#
# NOTE: multiple licenses have been detected; they have been separated with &
# in the LICENSE value for now since it is a reasonable assumption that all
# of the licenses apply. If instead there is a choice between the multiple
# licenses then you should change the value to separate the licenses with |
# instead of &. If there is any doubt, check the accompanying documentation
# to determine which situation is applicable.
LICENSE = "Unknown & GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=3f0c13d5177e348afee74a0a6765247c"

SRC_URI = "git://github.com/analogdevicesinc/aditof_sdk"

# Modify these as desired
#PV = "1.0+git${SRCPV}"
SRCREV = "143b5cc62664e07b5554a8c9c887a758e202cb64"

S = "${WORKDIR}/git"

# NOTE: unable to map the following CMake package dependencies: Matlab glog pybind11 OpenCV Protobuf Open3D
# NOTE: the following library dependencies are unknown, ignoring: libuvc
#       (this is based on recipes that have previously been built and packaged)
DEPENDS = "opencv protobuf-c-native libwebsockets31 protobuf39 glog35 zlib openssl"


# Specify any options you want to pass to cmake using EXTRA_OECMAKE:
EXTRA_OECMAKE = " -DWITH_EXAMPLES=on -DCMAKE_PREFIX_PATH='/opt/protobuf;/opt/websockets;'"
inherit cmake pkgconfig


