#!/bin/sh

path=~/var-fsl-yocto
wd=`pwd`

imx=$path/sources/meta-variscite-imx
dst=$imx/recipes-bsp/u-boot

rm -rf $dst/files
mkdir $dst/files
rm -f $dst/u-boot-fw-utils.bbappend
rm -f $dst/u-boot-variscite.bbappend

ln -s $wd/u-boot-bb/u-boot-fw-utils.bbappend $dst/u-boot-fw-utils.bbappend
ln -s $wd/u-boot-bb/fw-utils.diff $dst/files/fw-utils.diff

ln -s $wd/u-boot-bb/u-boot-variscite.bbappend $dst/u-boot-variscite.bbappend
ln -s $wd/u-boot-bb/variscite.diff $dst/files/variscite.diff

rm $imx/conf/machine/imx8mm-var-dart.conf
ln -s $wd/files/imx8mm-var-dart.conf $imx/conf/machine/imx8mm-var-dart.conf

rm -f $imx/recipes-kernel/linux/files
ln -s $wd/files/diffs $imx/recipes-kernel/linux/files
rm -f $imx/recipes-kernel/linux/linux-variscite_4.14.98.bbappend
ln -s $wd/linux-variscite_4.14.98.bbappend $imx/recipes-kernel/linux/linux-variscite_4.14.98.bbappend

echo "$imx/recipes-kernel/linux/linux-variscite_4.14.98.bbappend"
