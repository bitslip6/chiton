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

LICENSE = "CLOSED"

SRC_URI = "git://github.com/analogdevicesinc/aditof_sdk.git;protocol=https;branch=master"

# Modify these as desired
PV = "1.0+git${SRCPV}"
SRCREV = "7176257110c04595e45a3c03b86ab73f0b5b9c22"

S = "${WORKDIR}/git"

# NOTE: unable to map the following CMake package dependencies: Libwebsockets Protobuf Open3D OpenCV pybind11 Matlab
# NOTE: the following library dependencies are unknown, ignoring: libuvc
#       (this is based on recipes that have previously been built and packaged)
DEPENDS = "glog"

inherit cmake

# Specify any options you want to pass to cmake using EXTRA_OECMAKE:
EXTRA_OECMAKE = " -DWITH_NETWORK=OFF -DWITH_EXAMPLES=OFF  "

