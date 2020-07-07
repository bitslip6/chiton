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


SUMMARY = "install system configuration for chiton"
DESCRIPTION = "system configuration files and customization for chiton"

AUTHOR = "Cory Marsh, cory@bitslip6.com"

PRIORITY = "optional"
LICENSE = "GPL-3.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-3.0;md5=c79ff39f19dfec6d293b95dea7b07891"

PR = "r0"

SRC_URI = "file://init_chiton \
	file://blacklist.conf \
"

S = "${WORKDIR}/"

INITSCRIPT_NAME = "init_chiton"
INITSCRIPT_PARAMS = "-d"

# RDEPENDS_${PN} += "python-core"

FILES_${PN} = "\
           /etc/* \
          "
PACKAGES = " chiton"

#This needs to get the script into rc?.d/
inherit update-rc.d

do_install_append() {
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/init_chiton ${D}${sysconfdir}/init.d
    install -d ${D}${sysconfdir}/modprobe.d
    install -m 0755 ${WORKDIR}/blacklist.conf ${D}${sysconfdir}/modprobe.d
    #install -d ${D}${bindir}
    #install -m 0755 ${S}/init_tempmon.py ${D}${bindir}
}

