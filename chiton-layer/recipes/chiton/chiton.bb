# Recipe created by recipetool
# This is the basis of a recipe and may need further editing in order to be fully functional.
# (Feel free to remove these comments when editing.)

LICENSE = "CLOSED"

SRC_URI = "gitsm://git@github.com/bitslip6/chiton.git;protocol=ssh;branch=feature/master-devtool"
#SRC_URI = "gitsm:///home/murilo/meta-chiton/;protocol=file"
#FILESEXTRAPATHS_prepend := "/home/murilo/meta-chiton:"
#SRCBRANCH = "feature/yocto32"

# Modify these as desired
PV = "2"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

DEPENDS = "libjpeg-turbo aditof i2c-tools libinput libevdev libsdl2"

inherit cmake pkgconfig

# Specify any options you want to pass to cmake using EXTRA_OECMAKE:
EXTRA_OECMAKE = ""

