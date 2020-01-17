#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2019-2020 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
#
# [PEP-517](https://www.python.org/dev/peps/pep-0517/)
#     A build-system independent format for source trees
#     Specification for a build backend system.
#
#-------------------------------------------------------------------------------
import glob
import os
import platform
import sys
import textwrap
from ci import xbuild
from ci.setup_utils import get_datatable_version, make_git_version_file


def create_logger(verbosity):
    return (xbuild.Logger0() if verbosity == 0 else \
            xbuild.Logger1() if verbosity == 1 else \
            xbuild.Logger2() if verbosity == 2 else \
            xbuild.Logger3())


def build_extension(cmd, verbosity=3):
    assert cmd in ["asan", "build", "coverage", "debug"]
    arch = platform.machine()  # 'x86_64' or 'ppc64le'
    windows = (sys.platform == "win32")
    macos = (sys.platform == "darwin")
    linux = (sys.platform == "linux")
    ppc64 = ("ppc64" in arch or "powerpc64" in arch)
    if not (windows or macos or linux):
        print("\x1b[93mWarning: unknown platform %s\x1b[m" % sys.platform)
        linux = True

    ext = xbuild.Extension()
    ext.log = create_logger(verbosity)
    ext.name = "_datatable"
    ext.build_dir = "build/" + cmd
    ext.destination_dir = "src/datatable/lib/"
    ext.add_sources("src/core/**/*.cc")

    # Common compile settings
    ext.compiler.enable_colors()
    ext.compiler.add_include_dir("src/core")
    ext.compiler.add_default_python_include_dir()

    if ext.compiler.is_msvc():
        # Common compile flags
        ext.compiler.add_compiler_flag("/W4")
        ext.compiler.add_compiler_flag("/EHsc")
        ext.compiler.add_compiler_flag("/nologo")
        ext.compiler.add_include_dir(ext.compiler.path + "\\include")
        ext.compiler.add_include_dir(ext.compiler.winsdk_include_path + "\\ucrt")
        ext.compiler.add_include_dir(ext.compiler.winsdk_include_path + "\\shared")
        ext.compiler.add_include_dir(ext.compiler.winsdk_include_path + "\\um")

        # Common link flags
        ext.compiler.add_linker_flag("/nologo")
        ext.compiler.add_linker_flag("/DLL")
        ext.compiler.add_linker_flag("/EXPORT:PyInit__datatable")
        ext.compiler.add_default_python_lib_dir()
        ext.compiler.add_lib_dir(ext.compiler.path + "\\lib\\x64")
        ext.compiler.add_lib_dir(ext.compiler.winsdk_lib_path + "\\ucrt\\x64")
        ext.compiler.add_lib_dir(ext.compiler.winsdk_lib_path + "\\um\\x64")


        if cmd == "asan":
            raise RuntimeError("`make asan` is not supported on Windows systems")

        if cmd == "build":
            ext.compiler.add_compiler_flag("/O2")   # full optimization

        if cmd == "coverage":
            raise RuntimeError("`make coverage` is not supported on Windows systems")

        if cmd == "debug":
            xt.compiler.add_compiler_flag("/Od")    # no optimization
            ext.compiler.add_compiler_flag("/Z7")
            ext.compiler.add_linker_flag("/DEBUG:FULL")
    else:
        # Common compile flags
        ext.compiler.add_compiler_flag("-std=c++11")
        # "-stdlib=libc++"  (clang ???)
        ext.compiler.add_compiler_flag("-fPIC")

        # Common link flags
        ext.compiler.add_linker_flag("-shared")
        ext.compiler.add_linker_flag("-g")
        ext.compiler.add_linker_flag("-m64")
        if macos:
            ext.compiler.add_linker_flag("-undefined", "dynamic_lookup")
        if linux:
            ext.compiler.add_linker_flag("-lstdc++")
        if ppc64:
            ext.compiler.add_linker_flag("-pthread")

        if cmd == "asan":
            ext.compiler.add_compiler_flag("-fsanitize=address")
            ext.compiler.add_compiler_flag("-fno-omit-frame-pointer")
            ext.compiler.add_compiler_flag("-fsanitize-address-use-after-scope")
            ext.compiler.add_compiler_flag("-shared-libasan")
            ext.compiler.add_compiler_flag("-g3")
            ext.compiler.add_compiler_flag("-glldb" if macos else "-ggdb")
            ext.compiler.add_compiler_flag("-O0")
            ext.compiler.add_compiler_flag("-DDTTEST", "-DDTDEBUG")
            ext.compiler.add_linker_flag("-fsanitize=address", "-shared-libasan")

        if cmd == "build":
            ext.compiler.add_compiler_flag("-g2")  # include some debug info
            ext.compiler.add_compiler_flag("-O3")  # full optimization

        if cmd == "coverage":
            ext.compiler.add_compiler_flag("-g2")
            ext.compiler.add_compiler_flag("-O0")
            ext.compiler.add_compiler_flag("--coverage")
            ext.compiler.add_compiler_flag("-DDTTEST", "-DDTDEBUG")
            ext.compiler.add_linker_flag("-O0")
            ext.compiler.add_linker_flag("--coverage")

        if cmd == "debug":
            ext.compiler.add_compiler_flag("-g3")
            ext.compiler.add_compiler_flag("-fdebug-macro")
            ext.compiler.add_compiler_flag("-glldb" if macos else "-ggdb")
            ext.compiler.add_compiler_flag("-O0")  # no optimization
            ext.compiler.add_compiler_flag("-DDTTEST", "-DDTDEBUG")

        # Compiler warnings
        if ext.compiler.is_clang():
            ext.compiler.add_compiler_flag(
                "-Weverything",
                "-Wno-c++98-compat-pedantic",
                "-Wno-c99-extensions",
                "-Wno-exit-time-destructors",
                "-Wno-float-equal",
                "-Wno-global-constructors",
                "-Wno-reserved-id-macro",
                "-Wno-switch-enum",
                "-Wno-weak-template-vtables",
                "-Wno-weak-vtables",
            )
        else:
            ext.compiler.add_compiler_flag(
                "-Wall",
                "-Wno-unused-value",
                "-Wno-unknown-pragmas"
            )

    # Setup is complete, ready to build
    ext.build()
    return os.path.basename(ext.output_file)



def get_meta():
    return dict(
        name="datatable",
        version=get_datatable_version(),

        summary="Python library for fast multi-threaded data manipulation and "
                "munging.",
        description="""
            This is a Python package for manipulating 2-dimensional tabular data
            structures (aka data frames). It is close in spirit to pandas or SFrame;
            however we put specific emphasis on speed and big data support. As the
            name suggests, the package is closely related to R's data.table and
            attempts to mimic its core algorithms and API.

            See https://github.com/h2oai/datatable for more details.
        """,
        keywords=["datatable", "data", "dataframe", "frame", "data.table",
                  "munging", "numpy", "pandas", "data processing", "ETL"],

        # Author details
        author="Pasha Stetsenko",
        author_email="pasha.stetsenko@h2o.ai",
        maintainer="Oleksiy Kononenko",
        maintainer_email="oleksiy.kononenko@h2o.ai",

        home_page="https://github.com/h2oai/datatable",
        license="Mozilla Public License v2.0",
        classifiers=[
            "Development Status :: 4 - Beta",
            "Intended Audience :: Developers",
            "Intended Audience :: Science/Research",
            "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
            "Operating System :: MacOS",
            "Operating System :: Unix",
            "Programming Language :: Python :: 3.5",
            "Programming Language :: Python :: 3.6",
            "Programming Language :: Python :: 3.7",
            "Programming Language :: Python :: 3.8",
            "Topic :: Scientific/Engineering :: Information Analysis",
        ],

        # Runtime dependencies
        requirements=[
            "typesentry (>=0.2.6)",
            "blessed",
            "pytest (>=3.1); extra == 'tests'",
            "docutils (>=0.14); extra == 'tests'",
            "numpy; extra == 'optional'",
            "pandas; extra == 'optional'",
            "xlrd; extra == 'optional'",
        ],
        requires_python=">=3.5",
    )



#-------------------------------------------------------------------------------
# Standard hooks
#-------------------------------------------------------------------------------

def build_wheel(wheel_directory, config_settings=None, metadata_directory=None):
    """
    Function for building wheels, satisfies requirements of PEP-517.
    """
    if config_settings is None:
        config_settings = {}
    assert isinstance(wheel_directory, str)
    assert isinstance(config_settings, dict)
    assert metadata_directory is None

    so_file = build_extension(cmd="build", verbosity=3)
    files = glob.glob("src/datatable/**/*.py", recursive=True)
    files += ["src/datatable/include/datatable.h"]
    files += ["src/datatable/lib/" + so_file]
    files = [(f, f[4:])  # (src_file, destination_file)
             for f in files if "_datatable_builder.py" not in f]
    files.sort()

    meta = get_meta()
    wb = xbuild.Wheel(files, **meta, **config_settings)
    wb.log = create_logger(verbosity=3)
    wheel_file = wb.build_wheel(wheel_directory)
    return wheel_file



def build_sdist(sdist_directory, config_settings=None):
    """
    Function for building source distributions, satisfies PEP-517.
    """
    assert isinstance(sdist_directory, str)
    assert config_settings is None or isinstance(config_settings, dict)

    files = [f for f in glob.glob("src/datatable/**/*.py", recursive=True)
             if "_datatable_builder.py" not in f]
    files += glob.glob("src/core/**/*.cc", recursive=True)
    files += glob.glob("src/core/**/*.h", recursive=True)
    files += glob.glob("ci/xbuild/*.py")
    files += [f for f in glob.glob("tests/**/*.py", recursive=True)
              if "random_attack_logs" not in f]
    files += ["src/datatable/include/datatable.h"]
    files.sort()
    files += ["ext.py"]
    files += ["pyproject.toml"]
    files += ["LICENSE"]

    meta = get_meta()
    wb = xbuild.Wheel(files, **meta)
    wb.log = create_logger(verbosity=3)
    sdist_file = wb.build_sdist(sdist_directory)
    return sdist_file




#-------------------------------------------------------------------------------
# Allow this script to run from command line
#-------------------------------------------------------------------------------

def main():
    import argparse
    parser = argparse.ArgumentParser(
        description='Build _datatable module',
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("cmd", metavar="CMD",
        choices=["asan", "build", "coverage", "debug", "gitver", "sdist",
                 "wheel"],
        help=textwrap.dedent("""
            Specify what this script should do:

            asan     : build _datatable with Address Sanitizer enabled
            build    : build _datatable normally, with full optimization
            coverage : build _datatable in a mode suitable for coverage
                       testing
            debug    : build _datatable in debug mode, optimized for gdb
                       on Linux and for lldb on MacOS
            gitver   : generate __git__.py file
            sdist    : create source distribution of datatable
            wheel    : create wheel distribution of datatable
            """).strip())
    parser.add_argument("-v", dest="verbosity", action="count", default=1,
        help="Verbosity level of the output, specify the parameter up to 3\n"
             "times for maximum verbosity; the default level is 1.")
    parser.add_argument("-d", dest="destination", default="dist",
        help="Destination directory for `sdist` and `wheel` commands.")
    parser.add_argument("--audit", action="store_true",
        help="This flag can be used with cmd='wheel' only, on a Linux\n"
             "platform, which must have the 'auditwheel' external tool\n"
             "installed. If this flag is specified, then after building a\n"
             "wheel, it will be tested with the auditwheel. If the test\n"
             "succeeds, i.e. the wheel is found to be compatible with a\n"
             "manylinux* tag, then the wheel will be renamed to use the new\n"
             "tag. Otherwise, an error will be raised.")

    args = parser.parse_args()
    if args.cmd in ["sdist", "wheel"]:
        make_git_version_file(True)
    if args.audit and "linux" not in sys.platform:
        raise ValueError("Argument --audit can be used on a Linux platform "
                         "only, current platform is `%s`" % sys.platform)

    if args.cmd == "wheel":
        wheel_file = build_wheel(args.destination, {"audit": args.audit})
        assert os.path.isfile(os.path.join("dist", wheel_file))
    elif args.cmd == "sdist":
        sdist_file = build_sdist(args.destination)
        assert os.path.isfile(os.path.join("dist", sdist_file))
    elif args.cmd == "gitver":
        make_git_version_file(True)
    else:
        with open("src/datatable/lib/.xbuild-cmd", "wt") as out:
            out.write(args.cmd)
        build_extension(cmd=args.cmd, verbosity=args.verbosity)


if __name__ == "__main__":
    main()
