# SRC_URI += "file://dts2.diff"
SRC_URI += "file://addi9036.2.diff"
SRC_URI += "file://full.diff"
SRC_URI += "file://jabil_defconfig"

KERNEL_DEFCONFIG_imx8mm-var-dart = "${WORKDIR}/jabil_defconfig"
