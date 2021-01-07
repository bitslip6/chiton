# Recipe created by recipetool
# This is the basis of a recipe and may need further editing in order to be fully functional.
# (Feel free to remove these comments when editing.)

LICENSE = "CLOSED"

SRC_URI = "gitsm://git@github.com/google/glog.git;protocol=ssh;branch=master"

# Modify these as desired
PV = "1.0+git${SRCPV}"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

# NOTE: unable to map the following CMake package dependencies: gflags Unwind
# NOTE: the following library dependencies are unknown, ignoring: dbghelp
#       (this is based on recipes that have previously been built and packaged)
inherit cmake

# Specify any options you want to pass to cmake using EXTRA_OECMAKE:
EXTRA_OECMAKE = " -DWITH_GFLAGS=OFF -DBUILD_TESTING=OFF "

