#!/bin/sh

# create a group for i2c users/apps
if grep -q i2c /etc/group; then
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
	groupadd -r fb
	# create rules to apply fb perssions to devices on kernel updates
	echo 'KERNEL=="fb[0-9]*", GROUP="fb"' > /etc/udev/rules.d/10-local_fb_group.rules

	# reload and fire new udev rules
	udevadm control --reload-rules && udevadm trigger
fi



# create a user to chiton app
if grep -q chiton /etc/passwd; then
	useradd -m chiton

	# add chiton app to needed groups
	usermod -a -G i2c chiton
	usermod -a -G fb chiton
	usermod -a -G video chiton
	usermod -a -G input chiton

	addgroup chiton chiton
	useradd -p $(openssl passwd -1 "chiton") "root"
fi


# create systemd unit file to allow chiton autostart on boot
if test -f "/lib/systemd/system/chiton.service"; then
	echo "
	[Unit]
	Description=chiton
	After=network.target

	[Service]
	Restart=always
	Type=simple
	ExecStart=/usr/local/chiton/current/chiton
	User=chiton
	AmbientCapabilities=CAP_NET_BIND_SERVICE

	[Install]
	WantedBy=multi-user.target

	" >  /lib/systemd/system/chiton.service
fi


# reload systemd for changes in unit files
systemctl daemon-reload

# enable chiton for autostart on boot
systemctl enable chiton

# restart chiton app
systemctl start chiton


if ! grep -q 'unlimited' /etc/security/limits.conf; then
	echo "*		soft	core	unlimited" >> /etc/security/limits.conf
fi


if test -f "/etc/sudoers.d/20-chiton.conf"; then
	echo "%chiton ALL=(root) NOPASSWD: ALL" > /etc/sudoers.d/20-chiton.conf
fi

if grep -q i2c /etc/group; then
	groupadd -r i2c
fi



