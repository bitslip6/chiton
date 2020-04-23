#!/bin/bash

sum=`md5sum $1|cut -f1 -d' '`
touch $1.$sum
echo "created md5sum for $1"
