#!/bin/bash

cd /usr/local/chiton

hw=$(ifconfig eth0 | grep HWaddr | cut -d ' ' -f 11)

echo "Downloading"
curl -k "http://www.robotserver.net/chiton/rel_$1.tgz?hw=$hw" -o rel_$1.tgz
curl -k "http://www.robotserver.net/chiton/md5_$1.txt?hw=$hw" -o md5_$1.txt

sync
sync
sync

md1=$(cat md5_$1.txt)
md2=$(md5sum rel_$1.tgz)
if [[ "$md1" == "$md2" ]]; then
	tar zxf rel_$1.tgz
	sync
	sync

	cd rel_$1
	./setup.sh $1
	cd ..

	rm -f current
	ln -s rel_$1 current
	
	sync

	ldconfig


	echo "Chiton upgrade finished $1"
	sync
	/etc/init.d/reboot
	exit 0
else
	echo "Chiton download failed $1"
	exit 1
fi

