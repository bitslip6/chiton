#!/bin/sh

echo 117 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio117/direction

echo 118 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio118/direction

echo 119 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio119/direction

echo 121 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio121/direction

echo 120 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio120/direction

echo 122 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio122/direction

echo 123 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio123/direction

echo 89 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio89/direction

echo 85 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio85/direction

echo 86 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio86/direction

echo 87 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio87/direction

echo 88 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio88/direction
