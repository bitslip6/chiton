DESCRIPTION = "Protocol Buffers (a.k.a., protobuf) are Google's \
language-neutral, platform-neutral, extensible mechanism for \
serializing structured data."
HOMEPAGE = "https://github.com/protocolbuffers/protobuf"

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=37b5762e07f0af8c74ce80a8bda4266b"

DEPENDS = "zlib"

# file://0001-configure.ac-Allow-user-to-disable-gflags.patch 

SRC_URI = "https://github.com/protocolbuffers/protobuf/archive/v3.9.0.tar.gz"
SRC_URI[md5sum] = "d4a7f192d183b4970fafd34c4727b5b5"
SRC_URI[sha256sum] = "2ee9dcec820352671eb83e081295ba43f7a4157181dad549024d7070d079cf65"
BB_STRICT_CHECKSUM = "0"

S = "${WORKDIR}/protobuf-3.9.0"

FILES_${PN} += "/opt/protobuf/bin/*"
FILES_${PN}-dev += "/opt/protobuf/include/*"
FILES_${PN}-staticdev += "/opt/protobuf/lib/*"

PACKAGES = "${PN} ${PN}-dev ${PN}-staticdev ${PN}-doc ${PN}-dbg"

FILES_${PN} += " \
	${S}/cmake/ \
	${S}/glog-config.cmake.in \
"



# PACKAGECONFIG ??= ""
# PACKAGECONFIG[gflags] = ",--without-gflags,gflags,"
# -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/opt/protobuf

# we must install protobuf39 into /opt so as not to get in the way of the stock opencv protobuf :(
EXTRA_OECMAKE = " -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/opt/protobuf "
OECMAKE_SOURCEPATH ?= "../protobuf-3.9.0/cmake"

SYSROOT_DIRS += "/opt"


inherit cmake pkgconfig

