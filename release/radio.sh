#!/bin/sh

logf="/etc/radio.txt"
sleep 1
echo "configure wifi by editing /etc/radio.sh"

nmcli radio wifi off
sleep 2
nmcli radio wifi on
sleep 1
ifconfig wlan0 up
sleep 3


date >> $logf
connected=false
conn=false
echo "connecting to wireless network"
# connect to existing network
if test -f "/etc/radio_ssid"; then
	ssid=$(cat /etc/radio_ssid) 
	echo "ssid:  [$ssid]" >> $logf
	if test -f "/etc/radio_pass" && test -s "/etc/radio_pass"; then
		pass=$(cat /etc/radio_pass) 
		echo "pass:  $pass" >> $logf
		if test -f "/etc/NetworkManager/system-connections/${ssid}"; then
			if ! grep "$pass" "/etc/NetworkManager/system-connections/${ssid}"; then
				echo "nmcli connection up $ssid" >> $logf
				nmcli connection up "$ssid" >> $logf
				if [[ $? -eq 0 ]]; then
					echo "connection up true" >> $logf
					conn=true
				else
					echo "connection up failed" >> $logf
					sleep 2
					ifconfig >> $logf
				fi
			else
				echo "system-connection [$ssid] does not have password [$pass]" >> $logf
			fi
		else
			echo "no network configuration found" >> $logf
		fi
		if [ "$conn" = false ] ; then
			echo "removing any old network connections: ${ssid}*" >> $logf
			rm -f /etc/NetworkManager/system-connections/*
			cp /usr/local/chiton/current/jabil-orbital /etc/NetworkManager/system-connections/


			nmcli d wifi connect "$ssid" password "$pass"
			if [[ $? -eq 0 ]]; then
				conn=true
				echo "SUCCESS: connected to [$ssid]!\n" >> $logf
			else
				echo "FAILED: nmcli d wifi connect [$ssid] password [$pass]" >> $logf
			fi
			sleep 1
		fi
	else 
		echo "NO WIFI PASSWORD FOUND" >> $logf
	fi
else 
	echo "NO WIFI SSID CONFIGURED" >> $logf
fi


if [ "$conn" = false ] ; then
	echo "NOT CONNECTED!" >> $logf
	nmcli radio wifi off
	sleep 2
	nmcli radio wifi on
	sleep 1
	ifconfig wlan0 up >> $logf
	sleep 2

	nmcli connection up jabil-orbital >> $logf
else
	echo "IS CONNECTED!" >> $logf
fi

