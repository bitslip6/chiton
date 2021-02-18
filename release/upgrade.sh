#!/bin/bash

cd /usr/local/chiton

hw=$(ifconfig eth0 | grep HWaddr | cut -d ' ' -f 11)

echo "Downloading"
curl -k "http://www.robotserver.net/chiton/rel_$1.tgz?hw=$hw" -o rel_$1.tgz

tar zxf rel_$1.tgz

if [ ! -f current/chiton.json ]; then
	echo "creating chiton.json on current"
    cp -v current/config.json current/chiton.json
fi

if [ ! -f rel_$1/chiton.json ]; then
	echo "creating chiton.json on new release"
    cp -v rel_$1/config.json rel_$1/chiton.json
fi

echo "creating backups"
cp -v current/chiton.json current/chiton.json.bkp
cp -v rel_$1/chiton.json rel_$1/chiton.json.bkp

###Usage update_json <original_json_file> <new_json_file>
DIR="rel_$1"
echo "./update_json current/chiton.json ${DIR}/chiton.json"
#./update_json current/chiton.json ${DIR}/chiton.json
./update_json current/config.json ${DIR}/config.json

echo "cp config.json.new ${DIR}/chiton.json"
cp config.json.new ${DIR}/chiton.json
cp config.json.new ${DIR}/config.json

echo "cp ${DIR}/chiton.json ${DIR}/config.json"
cp ${DIR}/chiton.json "${DIR}"/config.json
#rm config.json.old

rm -f current

ln -s rel_$1 current

cd current
./setup.sh
cd ..

ldconfig

systemctl restart chiton
echo "Chiton restarted!!"