#!/bin/bash -x

docker build gcc10-toolchain -t chiton/gcc10cmake
docker run -v $PWD:/disney --workdir /disney chiton/gcc10cmake bash -x .github/workflows/ci.sh


