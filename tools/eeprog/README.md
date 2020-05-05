WHEN PROGRAMMING FROM KERNEL DRIVER:
====================================
use the kernelburn program to write a hex file to the eeprom using the kernel driver.
the i2cdetect program will return UU for id 56 and 57 when the kernel is driving the eeprom.
this program will write using the full 256 byte page write mechanism directly from the kernel.


TODO add this to the helpful_commands section: 
========================= (<gpio num = (port-1*32) + pin>)
echo 122 > /sys/class/gpio/export
sleep 1
echo out > /sys/class/gpio/gpio122/direction


This is the recommended method.

./kernelburn -w -b <bus_number> -i <id_number> -f <hex_file_to_write>


to read an eeprom from the kernel driver and create a hex dump file:
cat /sys/bus/i2c/devices/i2c-<bus_number>/<bus_number>-00<id_number>/eeprom|hexdump -v ><out_file.hex>
IE: i2c bus 3, device id 0x56:
cat /sys/bus/i2c/devices/i2c-3/3-0056/eeprom|hexdump -v >eeprom_56.hex




WHEN PROGRAMMING FROM USERSPACE:
================================
This programming method can be used when the kernel is not managing the eeprom
and the i2c devices can be seen from userspace via the i2cdetect program. 


Based on eeprog from Stefano Barbato, the program's original author


Compiling:
=========
requires make and gcc.
to compile type: "make" from this tool directory
you can force a recomile by: "make clean;make"


Using:
======
This tool comes with the ADI 3d TOF firmware with custom alpha and beta controls
and includes all data and software required to write the firmware and calibration
data to a new eeprom.


TLDR;
=====
this script will take about 2 minutes to complete
./burn_eeprom.sh


Making a copy of existing EEPROM:
=================================
The software has been modified to only allow 16 bit addressing.

to backup the eeprom at address 0x56 on bus 1 that is 131796 bytes in length:
./eeprog /dev/i2c-1 0x56 -16 -r 0:131796|hexdump -v > output_56.hex

to backup the eeprom at address 0x57 on bus 1 that is 131796 bytes in length:
./eeprog /dev/i2c-1 0x57 -16 -r 0:131796|hexdump -v > output_57.hex


Make an MD5 checksum of the firmware file:
==========================================
to make an md5 checksum for output_56.hex
./create_checksum output_56.hex

to make an md5 checksum for output_57.hex
./create_checksum output_57.hex


Write a firmware hex file to EEPROM
===================================

to write the output_56.hex file to the eeprom at address 0x56 on bus i2c-1:
./eeprog /dev/i2c-1 0x56 -16 -w 0 -m output_56.hex

to write the output_57.hex file to the eeprom at address 0x57 on bus i2c-1:
./eeprog /dev/i2c-1 0x57 -16 -w 0 -m output_57.hex





ORIGINAL README FILE
====================
Important! See the WARNING file.

eeprog reads and writes 24Cxx EEPROMs connected to I2C serial bus.

It uses the SMBus protocol used by most of the recent chipsets. Don't forget to load 
your i2c chipset and the i2c-dev drivers.

Use -8 switch for EEPROM smaller then 24C16 (8bit addressing mode). 

Again, it's really important that you read the WARNING file.

Type "make" to compile.
