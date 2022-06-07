#!/bin/sh

echo "CLEAN DEBUG BUILD"
make clean
export DEBUG_MODE=ON
bin/build.sh chiton
cp bin/chiton release/chiton-debug

echo "CLEAN RELEASE BUILD"
make clean
export DEBUG_MODE=OFF
bin/build.sh chiton
cp bin/chiton release/chiton
cp bin/api.so release/api.so

rm -rf rel_$1
cp -r release rel_$1
tar zcf rel_$1.tgz rel_$1

sleep 1

md5sum rel_$1.tgz > md5_$1.txt

scp -i /home/cory/.ssh/robotserver.pem rel_$1.tgz ubuntu@robotserver.net:/var/www/html/chiton/rel_$1.tgz
scp -i /home/cory/.ssh/robotserver.pem md5_$1.txt ubuntu@robotserver.net:/var/www/html/chiton/md5_$1.txt

rm -rf rel_$1

echo "RELEASE $1 BUILT"

