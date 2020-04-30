#!/bin/sh

# parallel interface new haven dispay port mapping 
# check output in dmesg for result.
# this should add a new /dev/fb device
# requires kernel modules fbtft, fbtft_device and tft_st7789v

sudo modprobe fbtft_device custom name=st7789v rotate=90 buswidth=8 fps=20 speed=16000000 txbuflen=-1 debug=$((1<<5)) gpios=reset:88,rd:87,wr:86,dc:85,db15:89,db14:123,db13:122,db12:120,db11:121,db10:119,db09:117,db08:118
