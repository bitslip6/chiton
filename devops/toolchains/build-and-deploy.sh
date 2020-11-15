#!/bin/bash -x

docker run -v $PWD:/disney --workdir /disney murilopontes/ubuntu1604_yocto bash -x .github/workflows/ci.sh


IP=192.168.1.103
scp -rp deploy_arm64/* $IP:/usr/local
ssh root@$IP systemctl restart chiton

