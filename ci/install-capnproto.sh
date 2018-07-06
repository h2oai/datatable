#!/bin/sh

set -ex

VERSION=0.6.1

curl -O https://capnproto.org/capnproto-c++-${VERSION}.tar.gz
tar zxf capnproto-c++-${VERSION}.tar.gz
cd capnproto-c++-${VERSION}
./configure
make -j6 check
SUDO=
if [ $(id -u) -ne 0 ]; then
    SUDO=sudo
fi

${SUDO} make install
${SUDO} ldconfig /usr/local/lib

cd .. && rm -rf capnproto-c++-${VERSION}*
