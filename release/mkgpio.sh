#!/bin/sh

mkdir /usr/local/chiton/debuginfo
chmod 777 /usr/local/chiton/debuginfo

# RESET_SAFETY, gpio4_00, 96
echo "RESET_SAFETY 96"
echo 96 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio96/direction
echo 1 > /sys/class/gpio/gpio96/value
# AFE_GPO2, gpio4_10, 106
#echo "106"
#echo 106 > /sys/class/gpio/export
#echo in > /sys/class/gpio/gpio106/direction
# AFE_GPO5, gpio4_11, 107
#echo "107"
#echo 107 > /sys/class/gpio/export
#echo in > /sys/class/gpio/gpio107/direction
# SOM_GPIO1, gpio4_23, 119
#echo "119"
#echo 119 > /sys/class/gpio/export
#echo in > /sys/class/gpio/gpio119/direction
# SOM_GPIO2, gpio4_26, 122
#echo "122"
#echo 122 > /sys/class/gpio/export
#echo in > /sys/class/gpio/gpio122/direction
# TEMP_INT_LASER, gpio5_29, 157
#echo "157"
#echo 157 > /sys/class/gpio/export
#echo in > /sys/class/gpio/gpio157/direction
# RSTB_SYNC, gpio5_28, 156
#echo "156"
#echo 156 > /sys/class/gpio/export
#echo in > /sys/class/gpio/gpio156/direction
# ENABLE_5V_VCSEL, gpio4_1,97 
#echo "97"
#echo 97 > /sys/class/gpio/export
#echo in > /sys/class/gpio/gpio97/direction
# SEL1, gpio5_04, 132
echo "SEL1 132"
echo 132 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio132/direction
echo 0 > /sys/class/gpio/gpio132/value
# SEL2, gpio5_3, 131
echo "SEL2 131"
echo 131 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio131/direction
echo 0 > /sys/class/gpio/gpio131/value
# SEL3, gpio1_15, 15
echo "SEL3 15"
echo 15 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio15/direction
echo 0 > /sys/class/gpio/gpio15/value
# SEL4, gpio1_14, 14
echo "SEL4 14"
echo 14 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio14/direction
echo 0 > /sys/class/gpio/gpio14/value
# SEL5, gpio4_20, 116
echo "SEL5 116"
echo 116 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio116/direction
echo 0 > /sys/class/gpio/gpio116/value
# SEL6, gpio1_6, 6
echo "SEL6 6"
echo 6 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio6/direction
echo 0 > /sys/class/gpio/gpio6/value
# SEL7, gpio1_05, 5
echo "SEL7 5"
echo 5 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio5/direction
echo 0 > /sys/class/gpio/gpio5/value
# SEL8, gpio1_1, 1
echo "SEL8 1"
echo 1 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio1/direction
echo 0 > /sys/class/gpio/gpio1/value
exit;

