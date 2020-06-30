DESCRIPTION = "The glog library implements application-level logging. This \
library provides logging APIs based on C++-style streams and various helper \
macros."
HOMEPAGE = "https://github.com/google/glog"

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://COPYING;md5=dc9db360e0bbd4e46672f3fd91dd6c4b"

DEPENDS = "libunwind"


#SRC_URI = "https://github.com/google/glog/archive/v0.3.5.tar.gz"
#SRC_URI[md5sum] = "5df6d78b81e51b90ac0ecd7ed932b0d4"
#SRC_URI[sha256sum] = "7580e408a2c0b5a89ca214739978ce6ff480b5e7d8d7698a2aa92fadc484d1e0"
#BB_STRICT_CHECKSUM = "0"

SRC_URI = " \
    git://github.com/google/glog.git;protocol=git;nobranch=1 \
    file://0001-configure.ac-Allow-user-to-disable-gflags.patch \
"

#SRCREV = "v0.3.5"
SRCREV = "a6a166db069520dbbd653c97c2e5b12e08a8bb26"

S = "${WORKDIR}/git"

PACKAGES = "${PN} ${PN}-dev ${PN}-staticdev ${PN}-doc ${PN}-dbg"
FILES_${PN} += "/usr/include/* \
	${S}/cmake/* \
	${S}/glog-config.cmake.in \
"

# PACKAGECONFIG ??= ""
# PACKAGECONFIG[gflags] = ",--without-gflags,gflags,"

# inherit cmake autotools pkgconfig
inherit cmake pkgconfig
