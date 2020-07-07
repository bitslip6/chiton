DESCRIPTION = "Libwebsockets is a simple-to-use, pure C library\
 providing client and server for http/1, http/2, websockets, MQTT\
 and other protocols in a security-minded, lightweight,\
 configurable, scalable and flexible way. It's easy to build and\
 cross-build via cmake and is suitable for tasks from embedded RTOS\
 through mass cloud serving."
HOMEPAGE = "https://github.com/protocolbuffers/protobuf"

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=4ce87f3facb6f911c142c8bef9bfb380"

DEPENDS = "zlib openssl"

# file://0001-configure.ac-Allow-user-to-disable-gflags.patch 

SRC_URI = "https://github.com/warmcat/libwebsockets/archive/v3.1.0.tar.gz"
SRC_URI[md5sum] = "325359a25d5f6d22725ff5d086db1c76"
SRC_URI[sha256sum] = "db948be74c78fc13f1f1a55e76707d7baae3a1c8f62b625f639e8f2736298324"
BB_STRICT_CHECKSUM = "0"

FILES_${PN} += "/opt/websockets/bin/* /usr/share/libwebsockets-test-server/*"
FILES_${PN}-dev += "/opt/websockets/include/*"
FILES_${PN}-staticdev += "/opt/websockets/lib/*"


S = "${WORKDIR}/libwebsockets-3.1.0"

# PACKAGECONFIG ??= ""
# PACKAGECONFIG[gflags] = ",--without-gflags,gflags,"
# EXTRA_OECMAKE = " -DLWS_STATIC_PIC=ON -DCMAKE_INSTALL_PREFIX=/opt/websockets"
# OECMAKE_SOURCEPATH ?= "../protobuf-3.9.0/cmake"
OECMAKE_GENERATOR = "Unix Makefiles"

inherit cmake pkgconfig

