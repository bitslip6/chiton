#!/bin/sh

path=/home/cory/var-fsl-yocto
wd=`pwd`

dst=$path/sources/meta-variscite-imx/recipes-bsp/u-boot

rm -rf $dst/files
mkdir $dst/files
rm -f $dst/u-boot-fw-utils.bbappend
rm -f $dst/u-boot-variscite.bbappend

ln -s $wd/u-boot-bb/u-boot-fw-utils.bbappend $dst/u-boot-fw-utils.bbappend
ln -s $wd/u-boot-bb/fw-utils.diff $dst/files/fw-utils.diff

ln -s $wd/u-boot-bb/u-boot-variscite.bbappend $dst/u-boot-variscite.bbappend
ln -s $wd/u-boot-bb/variscite.diff $dst/files/variscite.diff

