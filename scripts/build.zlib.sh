#!/bin/bash -e

ZLIB_VERSION="1.2.13"

mkdir -p deps
mkdir -p deps/include
mkdir -p deps/lib

mkdir -p build && cd build

wget https://zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz -O zlib-${ZLIB_VERSION}.tar.gz
tar -xzf zlib-${ZLIB_VERSION}.tar.gz

cd zlib-${ZLIB_VERSION}
./configure --static
make -j$(nproc || sysctl -n hw.ncpu || sysctl -n hw.logicalcpu)
cp -fr zlib.h zconf.h ../../deps/include
cp libz.a ../../deps/lib
cd ..
