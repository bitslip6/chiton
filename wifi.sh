#!/bin/sh
echo -n -e '\e[?17;14;224c'
sudo killall wpa_supplicant
sudo ifconfig $1 down
sudo iwconfig $1 mode ad-hoc
sudo iwconfig $1 chan 3
sudo iwconfig $1 essid chiton 
sudo ifconfig $1 inet $2
sudo ifconfig $1 up
