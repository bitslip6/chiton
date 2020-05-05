#!/bin/sh

i2cdetect -y 1
echo "removing exiting devices..."
echo 0x56 >/sys/bus/i2c/devices/i2c-1/device_delete
echo 0x57 >/sys/bus/i2c/devices/i2c-1/device_delete
i2cdetect -y 1

echo at24c 0x56 >/sys/bus/i2c/devices/i2c-1/new_device
echo at24c 0x57 >/sys/bus/i2c/devices/i2c-1/new_device

ls -l /sys/bus/i2c/devices/i2c-1/
