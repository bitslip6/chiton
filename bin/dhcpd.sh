#!/bin/sh
### BEGIN INIT INFO
# Provides:          rc.local
# Required-Start:    $all
# Required-Stop:
# Default-Start:     2 3 4 5
# Default-Stop:
# Short-Description: Run /etc/rc.local if it exist
### END INIT INFO

PATH=/sbin:/usr/sbin:/bin:/usr/bin
logf=/etc/radio.txt
date >> $logf
echo "starting dhcpd.sh" >> $logf
sleep 5
date >> $logf
echo "waited dhcpd.sh" >> $logf
if [ -x /usr/sbin/dhcpd ]; then
	ethconn=/sbin/ifconfig eth0 | /bin/grep "inet addr"
	/sbin/ifconfig eth0 | /bin/grep "inet addr"
	if [[ $? -eq 1 ]]; then
		echo "No Network found on eth0 [$ethconn].  Starting DHCPD on eth0" >> $logf
		ifconfig eth0 10.42.1.1
		sleep 1
	else
		echo "ETH0 found network, not starting DHCP server on ETH0" >> $logf
	fi

	/usr/sbin/dhcpd >> $logf
	echo "dhcpd started" >> $logf

else
	echo "no /usr/sbin/dhcpd found" >> $logf
fi

sync
exit 0
