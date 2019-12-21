#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2019 H2O.ai
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
import sys
import textwrap
from ci import xbuild


def create_logger(verbosity):
    return (xbuild.Logger0() if verbosity == 0 else \
            xbuild.Logger1() if verbosity == 1 else \
            xbuild.Logger2() if verbosity == 2 else \
            xbuild.Logger3())


def build_extension(cmd, verbosity=3):
    assert cmd in ["asan", "build", "coverage", "debug"]
    windows = (sys.platform == "win32")
    macos = (sys.platform == "darwin")
    linux = (sys.platform == "linux")
    if not (windows or macos or linux):
        print("\x1b[93mWarning: unknown platform %s\x1b[m" % sys.platform)
        linux = True

    ext = xbuild.Extension()
    ext.log = create_logger(verbosity)
    ext.name = "_datatable"
    ext.build_dir = "build/" + cmd
    ext.destination_dir = "datatable/lib/"
    ext.add_sources("c/**/*.cc")

    # Common compile settings
    ext.compiler.enable_colors()
    ext.compiler.add_include_dir("c")
    ext.compiler.add_default_python_include_dir()

    if ext.compiler.is_msvc():
        ext.compiler.add_compiler_flag("/W4")
        ext.compiler.add_compiler_flag("/EHsc")
        ext.compiler.add_compiler_flag("/O2")
        ext.compiler.add_compiler_flag("/nologo")

        ext.compiler.add_compiler_flag("/IC:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Tools\\MSVC\\14.24.28314\\include")
        ext.compiler.add_compiler_flag("/IC:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.18362.0\\ucrt")
        ext.compiler.add_compiler_flag("/IC:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.18362.0\\shared")
        ext.compiler.add_compiler_flag("/IC:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.18362.0\\um")

        ext.compiler.add_linker_flag("/nologo")
        ext.compiler.add_linker_flag("/DLL")
        ext.compiler.add_linker_flag("/LIBPATH:C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Tools\\MSVC\\14.24.28314\\lib\\x64")
        ext.compiler.add_linker_flag("/LIBPATH:C:\\Users\\jenkins\\AppData\\Local\\Programs\\Python\\Python36\\libs")
        ext.compiler.add_linker_flag("/LIBPATH:C:\\Program Files (x86)\\Windows Kits\\10\\lib\\10.0.18362.0\\ucrt\\x64")
        ext.compiler.add_linker_flag("/LIBPATH:C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.18362.0\\um\\x64\\")

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
        version="0.10.1",

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
    assert isinstance(wheel_directory, str)
    assert metadata_directory is None

    so_file = build_extension(cmd="build", verbosity=3)
    files = glob.glob("datatable/**/*.py", recursive=True)
    files += ["datatable/include/datatable.h"]
    files += ["datatable/lib/" + so_file]
    files = [f for f in files if "_datatable_builder.py" not in f]
    files.sort()

    meta = get_meta()
    wb = xbuild.Wheel(files, **meta)
    wb.log = create_logger(verbosity=3)
    wheel_file = wb.build_wheel(wheel_directory)
    return wheel_file



def build_sdist(sdist_directory, config_settings=None):
    """
    Function for building source distributions, satisfies PEP-517.
    """
    assert isinstance(sdist_directory, str)

    files = [f for f in glob.glob("datatable/**/*.py", recursive=True)
             if "_datatable_builder.py" not in f]
    files += glob.glob("c/**/*.cc", recursive=True)
    files += glob.glob("c/**/*.h", recursive=True)
    files += glob.glob("ci/xbuild/*.py")
    files += [f for f in glob.glob("tests/**/*.py", recursive=True)
              if "random_attack_logs" not in f]
    files += ["datatable/include/datatable.h"]
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
        choices=["asan", "build", "coverage", "debug", "sdist", "wheel"],
        help=textwrap.dedent("""
            Specify what this script should do:

            asan     : build _datatable with Address Sanitizer enabled
            build    : build _datatable normally, with full optimization
            coverage : build _datatable in a mode suitable for coverage
                       testing
            debug    : build _datatable in debug mode, optimized for gdb
                       on Linux and for lldb on MacOS
            sdist    : create source distribution of datatable
            wheel    : create wheel distribution of datatable
            """).strip())
    parser.add_argument("-v", dest="verbosity", action="count", default=1,
            help="Verbosity level of the output, specify the parameter up to \n"
                 "3 times for maximum verbosity; the default level is 1")

    args = parser.parse_args()
    if args.cmd == "wheel":
        wheel_file = build_wheel("dist/")
        assert os.path.isfile(os.path.join("dist", wheel_file))
    elif args.cmd == "sdist":
        sdist_file = build_sdist("dist/")
        assert os.path.isfile(os.path.join("dist", sdist_file))
    else:
        with open("datatable/lib/.xbuild-cmd", "wt") as out:
            out.write(args.cmd)
        build_extension(cmd=args.cmd, verbosity=args.verbosity)


if __name__ == "__main__":
    main()
