# Copy this file and Run from one level higher than the git pull directory
# To build: docker build -t opsh2oai/h2oai-nv -f Dockerfile-nvdocker .
# To run with docker run -it -p 12345:12345 opsh2oai/h2oai-nv

FROM nvidia/cuda:8.0-cudnn5-devel-ubuntu16.04
MAINTAINER H2o.ai <ops@h2o.ai>

ENV DEBIAN_FRONTEND noninteractive
ENV LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH

RUN \
  apt-get -y update && \
  apt-get -y install \
  curl \
  apt-utils \
  wget \
  cpio \
  python-software-properties \
  software-properties-common \
  vim \
  git \
  dirmngr

# Setup Repos
RUN \
  add-apt-repository ppa:fkrull/deadsnakes  && \
  apt-get update -yqq && \
  echo debconf shared/accepted-oracle-license-v1-1 select true | debconf-set-selections && \
  echo debconf shared/accepted-oracle-license-v1-1 seen true | debconf-set-selections && \
  curl -sL https://deb.nodesource.com/setup_7.x | bash -

# Install datatable dependencies
RUN \
  apt-get install -y \
  python3.6 \
  python3.6-dev \
  python3-pip \
  python3-dev \
  python-virtualenv \
  python3-virtualenv


RUN \
  virtualenv --python=/usr/bin/python3.6  .
  source /bin/activate
  /usr/bin/python3.6 -m pip install --upgrade pip && \
  /usr/bin/python3.6 -m pip install --upgrade setuptools && \
  /usr/bin/python3.6 -m pip install --upgrade python-dateutil && \
  /usr/bin/python3.6 -m pip install --upgrade numpy && \
  /usr/bin/python3.6 -m pip install --upgrade colorama && \
  /usr/bin/python3.6 -m pip install --upgrade typesentry && \
  /usr/bin/python3.6 -m pip install --upgrade wcwidth && \
  /usr/bin/python3.6 -m pip install --upgrade blessed && \
  /usr/bin/python3.6 -m pip install --upgrade llvmlite && \
  /usr/bin/python3.6 -m pip install --upgrade psutil && \
  /usr/bin/python3.6 -m pip install --upgrade pytest

# Remove apt-cache
RUN \
  apt-get clean && \
  rm -rf /var/cache/apt/*

# Install LLVM for pydatatable
RUN \
  wget http://releases.llvm.org/4.0.0/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz && \
  tar xf clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz && \
  rm clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz && \
  cp /clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.04/lib/libomp.so /usr/lib

ENV \
  LLVM4=/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.04 \
  LLVM_CONFIG=/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.04/bin/llvm-config \
  CC=/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang \
  CLANG=/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang



