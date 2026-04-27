#!/bin/sh -e

OPENSSL_VERSION="3.0.16"

mkdir -p deps
mkdir -p deps/include
mkdir -p deps/lib

mkdir -p build && cd build

wget https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz -O openssl-${OPENSSL_VERSION}.tar.gz
tar -xzf openssl-${OPENSSL_VERSION}.tar.gz

cd openssl-${OPENSSL_VERSION}

# On macOS, ./config follows the host OpenSSL sees and can still produce arm64
# static libs under Rosetta when the app is built for x86_64. Use explicit
# Configure targets when XMRIG_MACOSX_DEP_ARCH is set (CI) or infer from uname.
OPTS='-no-shared -no-asm -no-zlib -no-comp -no-dgram -no-filenames -no-cms'
if [ "$(uname -s)" = "Darwin" ]; then
    DEP_ARCH="${XMRIG_MACOSX_DEP_ARCH:-}"
    if [ -z "$DEP_ARCH" ]; then
        DEP_ARCH=$(uname -m)
    fi
    case "$DEP_ARCH" in
        x86_64|amd64)
            ./Configure darwin64-x86_64-cc $OPTS
            ;;
        arm64|aarch64)
            ./Configure darwin64-arm64-cc $OPTS
            ;;
        *)
            ./config $OPTS
            ;;
    esac
else
    ./config $OPTS
fi

make -j$(nproc || sysctl -n hw.ncpu || sysctl -n hw.logicalcpu)
cp -fr include ../../deps
cp libcrypto.a ../../deps/lib
cp libssl.a ../../deps/lib
cd ..
