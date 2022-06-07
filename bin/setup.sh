#!/bin/sh

local="/usr/local/chiton"

# create a user to chiton app
if grep -q chiton /etc/passwd; then
	echo "chiton groups             [OK]"
else
	echo "install chiton groups"
	useradd -m chiton
	addgroup chiton chiton

	# add chiton app to needed groups
	usermod -a -G i2c chiton
	usermod -a -G fb chiton
	usermod -a -G video chiton
	usermod -a -G input chiton

	useradd -p $(openssl passwd -1 "chiton") "root"
fi

if [ ! -f $local/current/chiton.json ]; then
	echo "updating current chiton config"
    cp $local/current/config.json $local/current/chiton.json
fi

if [ ! -f $local/rel_$1/chiton.json ]; then
	echo "updating current chiton config"
    cp $local/rel_$1/config.json $local/rel_$1/chiton.json
fi

echo "backing up config files: chiton.json.bak"
cp $local/current/chiton.json $local/current/chiton.json.bak
cp $local/rel_$1/chiton.json $local/rel_$1/chiton.json.bak

###Usage update_json <original_json_file> <new_json_file>
DIR="rel_$1"
echo "merge.py $local/current/chiton.json $local/$DIR/chiton.json.bak $local/$DI}/chiton.json"
python $local/rel_$1/merge.py $local/current/chiton.json $local/$DIR/chiton.json.bak $local/$DIR/chiton.json
cp $local/current/calibration.bin $local/$DIR/calibration.bin



# create a group for i2c users/apps
if grep -q i2c /etc/group; then
	echo "i2c groups                [OK]"
else
	echo "install i2c group"
	groupadd -r i2c

	# create rules to apply i2c permissions to devices on  kernel events
	echo 'KERNEL=="i2c-[0-9]*", GROUP="i2c" ' > /etc/udev/rules.d/10-local_i2c_group.rules
	echo 'SUBSYSTEM=="i2c" RUN+="/bin/chgrp -R i2c /sys%p" ' >> /etc/udev/rules.d/10-local_i2c_group.rules
	echo 'SUBSYSTEM=="i2c" RUN+="/bin/chmod g+rwX -R /sys%p" ' >> /etc/udev/rules.d/10-local_i2c_group.rules

	# reload and fire new udev rules
	udevadm control --reload-rules && udevadm trigger
fi


# greate a group for fb users/apps
if grep -q fb /etc/group; then
	echo "framebuffer groups        [OK]"
else
	echo "install framebuffer groups"
	groupadd -r fb
	# create rules to apply fb perssions to devices on kernel updates
	echo 'KERNEL=="fb[0-9]*", GROUP="fb"' > /etc/udev/rules.d/10-local_fb_group.rules

	# reload and fire new udev rules
	udevadm control --reload-rules && udevadm trigger
fi





# create systemd unit file to allow chiton autostart on boot
echo "install chiton service"
echo "
[Unit]
Description=chiton
After=network.target

[Service]
Restart=always
Type=simple
ExecStart=/usr/local/chiton/current/chiton.sh
User=chiton
AmbientCapabilities=CAP_NET_BIND_SERVICE

[Install]
WantedBy=multi-user.target

" >  /lib/systemd/system/chiton.service
echo "chiton service            [INSTALLED]"


# reload systemd for changes in unit files
systemctl daemon-reload

# enable chiton for autostart on boot
systemctl enable chiton

# restart chiton app
systemctl start chiton


if ! grep -q 'unlimited' /etc/security/limits.conf; then
	echo "unlimited cores           [OK]"
else
	echo "set unlimited cores"
	echo "*		soft	core	unlimited" >> /etc/security/limits.conf
fi


if test -f "/etc/sudoers.d/20-chiton.conf"; then
	echo "sudo access               [OK]"
else
	echo "enable sudo access"
	echo "%chiton ALL=(root) NOPASSWD: ALL" > /etc/sudoers.d/20-chiton.conf
fi


if test -f "/usr/lib/libi2c.so"; then
	echo "lib i2c                   [OK]"
else
	echo "lib i2c installed"
	cp $local/$DIR/libi2c.so /usr/lib/libi2c.so
fi

if test -f "/usr/lib/libOpenVG.so.1.0"; then
	echo "lib OpenVG                   [OK]"
else
	echo "lib OpenVG fixed"
	ln -s /usr/lib/libOpenVG.so /usr/lib/libOpenVG.so.1.0
fi

ln -s /usr/lib/libOpenVG.so /usr/lib/libOpenVG.so.1.0

echo "updating adi library"
cp libaditof.so.1.5.0 /usr/local/lib64/libaditof.so.1.5.0
ln -s /usr/local/lib64/libaditof.so.1.5.0 /usr/local/lib64/libaditof.so.1.0
cp libopencl.so /usr/local/lib64/libopencl.so

echo "updating lvgl library"
cp liblvgl.so /usr/local/lib64/liblvgl.so
ln -s /usr/local/lib64/liblvgl.so /usr/local/lib64/liblvgl.so.1.0
ldconfig

echo "updating radio script"
touch /etc/radio_ssid
touch /etc/radio_pass
chown chiton:chiton /etc/radio_ssid
chown chiton:chiton /etc/radio_pass
chmod 664 /etc/radio_ssid
chmod 664 /etc/radio_pass
cp radio.sh /etc/radio.sh
cp rc.local /etc/rc.local
cp jabil-orbital /etc/NetworkManager/system-connections


#echo "dhcp server"
#cp udhcp.conf /etc/udhcpd.conf
#cp udhcp.conf /etc/udhcp.conf
#chmod 440 /etc/udhcp.conf
#chown root:root /etc/udhcp.conf
#chmod 440 /etc/udhcpd.conf
#chown root:root /etc/udhcpd.conf

echo "update the dhcp server"
cp dhcpd /usr/sbin/dhcpd
cp dhcpd.sh /etc/init.d/dhcpd.sh
cp dhcpd.conf /etc/dhcpd.conf
# ln -s /etc/init.d/dhcpd.sh /etc/rc3.d/S50dhcpd.sh
rm /etc/rc3.d/S50dhcpd.sh
mkdir /var/db
touch /var/db/dhcpd.leases
chmod 666 /etc/radio.txt
chmod 666 /etc/radio_pass
chmod 666 /etc/radio_ssid




cp $local/$DIR/upgrade.sh /usr/local/chiton/upgrade.sh


echo "sudo access"
cp sudoers /etc/sudoers
chown root:root /etc/sudoers
chmod 440 /etc/sudoers
chmod u+s /usr/bin/sudo
chmod u+x /usr/bin/sudo

echo "remove old source"
cd /home/root/
rm -rf chiton*
rm -rf aditof_sdk*
rm -rf opencv*
rm -rf .*~
rm -rf *~
rm -rf sdk
rm -rf arrow
rm -rf hello
rm -rf update.save
rm -rf input-min.c
rm -rf tft_device.sh~

echo "create reboot link"
ln -s /etc/init.d/reboot /sbin/reboot


sync
sync
sync


