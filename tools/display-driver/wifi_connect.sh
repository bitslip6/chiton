#!/bin/sh

#
# use the network manager command line application to easily connect to a wifi network
# replace <essid> and <password> with the respective essid and password to connect to
nmcli d wifi connect <essid> password <password>

#
# request a webpage from a local web server, use the web log to find the IP address of the new system
# you can also use nmap to probe for network devices on the local network.
# look for a system with ssh open
# replace <web.server.ip> with the ip address of the local webserver, then check the access logs.
# IE: grep 'imx8mm' /var/log/http/httpd.log (your milage may vary)
wget http://<web.server.ip>/imx8mm.txt


