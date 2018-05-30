#!/bin/bash

if [ ! -d ".asan" ]; then
    PYVER=`python -c "import sys; print(sys.version[:3])"`
    TARGET=".asan/lib/python$PYVER/site-packages"

    virtualenv .asan --python=python$PYVER
    echo -e "\nexport DT_ASAN_TARGETDIR=\"$TARGET\"\n" >> .asan/bin/activate

    DT_ASAN_TARGETDIR=$TARGET \
        python ci/asan-pip.py

    LLVM="$LLVM4$LLVM5$LLVM6"
    CC="$LLVM/bin/clang"
    PYCONF="python$PYVER-config"
    LDFLAGS="$LDFLAGS `$PYCONF --ldflags`"
    CFLAGS="$CFLAGS `$PYCONF --cflags` -fsanitize=address"

    DT_ASAN_TARGETDIR=$TARGET \
        $CC $CFLAGS $LDFLAGS -o .asan/bin/mypython ci/asan-python.c

    chmod +x .asan/bin/mypython
    cp .asan/bin/mypython .asan/bin/python

    source .asan/bin/activate
    pip install colorama
    pip install typesentry
    pip install blessed
    pip install llvmlite
    pip install pytest
    pip install pandas
fi

