# Copy this file and Run from one level higher than the git pull directory
# To build: docker build -t opsh2oai/h2oai-nv -f Dockerfile-nvdocker .
# To run with docker run -it -p 12345:12345 opsh2oai/h2oai-nv

FROM nvidia/cuda:8.0-cudnn5-devel-ubuntu16.04
MAINTAINER H2o.ai <ops@h2o.ai>

ENV DEBIAN_FRONTEND noninteractive
ENV LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH

RUN \
  apt-get -q -y update && \
  apt-get -y --no-install-recommends  install \
    apt-utils \
    curl \
    wget \
    cpio \
    git \
    python-software-properties \
    software-properties-common && \
  add-apt-repository ppa:jonathonf/python-3.6 && \
  apt-get -q -y update && \
  apt-get -y --no-install-recommends  install \
    python3.6 \
    python3.6-dev \
    python3.6-venv \
    python3-pip && \
  update-alternatives --install /usr/bin/python python /usr/bin/python3.6 100 && \
  python -m pip install --upgrade pip && \
  apt-get clean && \
  rm -rf /var/cache/apt/*

RUN \
  pip install --upgrade pip && \
  pip install --upgrade setuptools && \
  pip install --upgrade python-dateutil && \
  pip install --upgrade numpy && \
  pip install --upgrade colorama && \
  pip install --upgrade typesentry && \
  pip install --upgrade wcwidth && \
  pip install --upgrade blessed && \
  pip install --upgrade llvmlite && \
  pip install --upgrade psutil && \
  pip install --upgrade pandas && \
  pip install --upgrade pytest && \
  pip install --upgrade virtualenv && \
  pip install --upgrade wheel

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

