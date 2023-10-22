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
# [PEP-440](https://www.python.org/dev/peps/pep-0440/)
#     Description of standard version formats.
#
#-------------------------------------------------------------------------------
import glob
import os
import platform
import re
import subprocess
import sys
import textwrap
import time
import xbuild
import typing
#-------------------------------------------------------------------------------
# Version handling
#-------------------------------------------------------------------------------

def is_source_distribution():
    return not os.path.exists("VERSION.txt") and \
           os.path.exists("src/datatable/_build_info.py")


# The primary source of datatable's release version is the file
# VERSION.txt in the root of the repository.
#
# When building the release version of datatable, this file is
# expected to contain the "release version" of the distribution,
# i.e. have form
#
#     XX.YY.ZZ
#
# In all other cases, the file is expected to contain the main
# version + optional suffix "a", "b" or "rc":
#
#     XX.YY.ZZ[a|b|rc]
#
# If the suffix is absent, then this is presumed to be the
# "post-release" version, and suffix `.post` is automatically added.
#
# This procedure verifies that the content of VERSION.txt is in
# the appropriate format, and returns the augmented version of the
# datatable distribution:
#
# - In release mode (env.variable DT_RELEASE is set), the final
#   release is the same as the content of VERSION.txt;
#
# - In PR mode (env.variable DT_BUILD_SUFFIX is present), the final
#   version is `VERSION.txt` "0+" `DT_BUILD_SUFFIX`
#
# - In dev-main mode (env.variable DT_BUILD_NUMBER is present),
#   the version is equal to `VERSION.txt` BUILD_NUMBER;
#
# - When building from source distribution (file VERSION.txt is
#   absent, the version is taken from datatable/_build_info.py) file;
#
# - In all other cases (local build), the final version consists of
#   of `VERSION.txt` "0+" [buildmode "."] timestamp ["." username].
#
def get_datatable_version(flavor=None):
    build_mode = "release" if os.environ.get("DT_RELEASE") else \
                 "PR" if os.environ.get("DT_BUILD_SUFFIX") else \
                 "dev" if os.environ.get("DT_BUILD_NUMBER") else \
                 "sdist" if is_source_distribution() else \
                 "local"

    if build_mode != "sdist":
        if not os.path.exists("VERSION.txt"):
            raise SystemExit("File VERSION.txt is missing when building "
                             "datatable in %s mode" % build_mode)
        with open("VERSION.txt", "r") as inp:
            version = inp.read().strip()

    # In release mode, the version is just the content of VERSION.txt
    if build_mode == "release":
        if not re.fullmatch(r"\d+(\.\d+)+", version):
            raise SystemExit("Invalid version `%s` in VERSION.txt when building"
                             " datatable in release mode (DT_RELEASE is on)"
                             % version)
        if flavor == "debug":
            version += "+debug"
        elif flavor not in [None, "sdist", "build"]:
            raise SystemExit("Invalid build flavor %s when building datatable "
                             "in release mode" % flavor)
        return version

    # In PR mode, the version is appended with DT_BUILD_SUFFIX
    if build_mode == "PR":
        suffix = os.environ.get("DT_BUILD_SUFFIX")
        if not re.fullmatch(r"\w([\w\.]*\w)?", suffix):
            raise SystemExit("Invalid build suffix `%s` from environment "
                             "variable DT_BUILD_SUFFIX" % suffix)
        mm = re.fullmatch(r"\d+(\.\d+)+(a|b|rc)?", version)
        if not mm:
            raise SystemExit("Invalid version `%s` in VERSION.txt when building"
                             " datatable in PR mode" % version)
        if not mm.group(2):
            version += "a"
        version += "0+" + suffix.lower()
        if flavor == "debug":
            version += ".debug"
        return version

    # In "main-dev" mode, the DT_BUILD_NUMBER is used
    if build_mode == "dev":
        build = os.environ.get("DT_BUILD_NUMBER")
        if not re.fullmatch(r"\d+", build):
            raise SystemExit("Invalid build number `%s` from environment "
                             "variable DT_BUILD_NUMBER" % build)
        mm = re.fullmatch(r"\d+(\.\d+)+(a|b|rc)?", version)
        if not mm:
            raise SystemExit("Invalid version `%s` in VERSION.txt when building"
                             " datatable in development mode" % version)
        if not mm.group(2):
            version += ".post"
        version += build
        if flavor == "debug":
            version += "+debug"
        return version

    # Building from sdist (file VERSION.txt not included)
    if build_mode == "sdist":
        return _get_version_from_build_info()

    # Otherwise we're building from a local distribution
    if build_mode == "local":
        if not version[-1].isdigit():
            version += "0"
        version += "+"
        if flavor:
            version += flavor + "."
        version += str(int(time.time()))
        user = _get_user()
        if user:
            version += "." + user
        return version



def _get_version_from_build_info():
    info_file = os.path.join("src", "datatable", "_build_info.py")
    if not os.path.exists(info_file):
        raise SystemExit("Invalid source distribution: file "
                         "src/datatable/_build_info.py is missing")
    with open(info_file, "r", encoding="utf-8") as inp:
        text = inp.read()
    mm = re.search(r"\s*version\s*=\s*['\"]([\w\+\.]+)['\"]", text)
    if not mm:
        raise SystemExit("Cannot find version in src/datatable/"
                         "_build_info.py file")
    return mm.group(1)



def _get_user():
    import getpass
    try:
        user = getpass.getuser()
        return re.sub(r"[^a-zA-Z0-9]+", "", user)
    except KeyError:
        # An exception may be raised if the user is not in /etc/passwd file
        return ""



#-------------------------------------------------------------------------------
# Commands implementation
#-------------------------------------------------------------------------------

def create_logger(verbosity):
    return (xbuild.Logger0() if verbosity == 0 else \
            xbuild.Logger1() if verbosity == 1 else \
            xbuild.Logger2() if verbosity == 2 else \
            xbuild.Logger3())


def build_extension(cmd, verbosity=3):
    assert cmd in ["asan", "build", "coverage", "debug"]
    arch = platform.machine()  # 'x86_64' or 'ppc64le' or 'arm64' or 'aarch64'
    ppc64 = ("ppc64" in arch or "powerpc64" in arch)
    arm64 = (arch == "arm64") or (arch == "aarch64")

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
    ext.destination_dir = "src/datatable/lib/"
    ext.add_sources("src/core/**/*.cc")

    # Common compile settings
    ext.compiler.enable_colors()
    ext.compiler.add_include_dir("src/core")
    ext.compiler.add_default_python_include_dir()

    if ext.compiler.is_msvc():
        # General compiler flags
        ext.compiler.add_compiler_flag("/std:c++14")
        ext.compiler.add_compiler_flag("/EHsc")
        ext.compiler.add_compiler_flag("/nologo")
        ext.compiler.add_include_dir(ext.compiler.path + "\\include")
        ext.compiler.add_include_dir(ext.compiler.winsdk_include_path + "\\ucrt")
        ext.compiler.add_include_dir(ext.compiler.winsdk_include_path + "\\shared")
        ext.compiler.add_include_dir(ext.compiler.winsdk_include_path + "\\um")


        # Set up the compiler warning level
        ext.compiler.add_compiler_flag("/W4")

        # Disable particular warnings
        ext.compiler.add_compiler_flag(
            # "This function or variable may be unsafe"
            # issued by MSVC for a fully valid and portable code
            "/wd4996",
            # "consider using 'if constexpr' statement instead"
            # as 'if constexpr' is not available in C++14
            "/wd4127",
            # "no suitable definition provided for explicit template instantiation
            # request" as we want to keep some template method definitions
            # in separate translation units
            "/wd4661",
            # "structure was padded due to alignment specifier"
            # as this is exactly the reason why we use the alignment specifier
            "/wd4324",
        )

        # Link flags
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
            ext.compiler.add_compiler_flag("/Od")    # no optimization
            ext.compiler.add_compiler_flag("/Z7")
            ext.compiler.add_compiler_flag("/DDT_TEST")
            ext.compiler.add_compiler_flag("/DDT_DEBUG")
            ext.compiler.add_linker_flag("/DEBUG:FULL")
    else:
        # Common compile flags
        ext.compiler.add_compiler_flag("-std=c++14")
        # "-stdlib=libc++"  (clang ???)
        ext.compiler.add_compiler_flag("-fPIC")
        # -pthread is recommended for compiling/linking multithreaded apps
        ext.compiler.add_compiler_flag("-pthread")
        ext.compiler.add_linker_flag("-pthread")

        # Common link flags
        ext.compiler.add_linker_flag("-shared")
        ext.compiler.add_linker_flag("-g")

        if not arm64:
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
            ext.compiler.add_compiler_flag("-DDT_TEST", "-DDT_DEBUG")
            ext.compiler.add_linker_flag("-fsanitize=address", "-shared-libasan")

        if cmd == "build":
            ext.compiler.add_compiler_flag("-g2")  # include some debug info
            ext.compiler.add_compiler_flag("-O3")  # full optimization

        if cmd == "coverage":
            ext.compiler.add_compiler_flag("-g2")
            ext.compiler.add_compiler_flag("-O0")
            ext.compiler.add_compiler_flag("--coverage")
            ext.compiler.add_compiler_flag("-DDT_TEST", "-DDT_DEBUG")
            ext.compiler.add_linker_flag("-O0")
            ext.compiler.add_linker_flag("--coverage")

        if cmd == "debug":
            ext.compiler.add_compiler_flag("-g3")
            ext.compiler.add_compiler_flag("-glldb" if macos else "-ggdb")
            ext.compiler.add_compiler_flag("-O0")  # no optimization
            ext.compiler.add_compiler_flag("-DDT_TEST", "-DDT_DEBUG")
            if ext.compiler.flavor == "clang":
                ext.compiler.add_compiler_flag("-fdebug-macro")

        # Compiler warnings
        if ext.compiler.is_clang():
            ext.compiler.add_compiler_flag(
                "-Weverything",
                "-Wno-c++98-compat-pedantic",
                "-Wno-c99-extensions",
                "-Wno-disabled-macro-expansion",
                "-Wno-exit-time-destructors",
                "-Wno-float-equal",
                "-Wno-global-constructors",
                "-Wno-reserved-id-macro",
                "-Wno-switch-enum",
                "-Wno-poison-system-directories",
                "-Wno-unknown-warning-option",
                "-Wno-weak-template-vtables",
                "-Wno-poison-system-directories",
                "-Wno-weak-vtables",
                "-Wno-unknown-warning-option",
            )
        else:
            ext.compiler.add_compiler_flag(
                "-Wall",
                "-Wno-unused-value",
                "-Wno-unknown-pragmas"
            )

    ext.add_prebuild_trigger(generate_documentation)

    # Setup is complete, ready to build
    ext.build()
    return ext.output_file


def generate_documentation(ext):
    hfile = "src/core/documentation.h"
    ccfile = "src/core/documentation.cc"
    docfiles = glob.glob("docs/api/**/*.rst", recursive=True)
    if ext.is_modified(hfile):
        import gendoc
        ext.log.report_generating_docs(ccfile)
        gendoc.generate_documentation(hfile, ccfile, docfiles)
        ext.add_sources(ccfile)



def get_meta():
    return dict(
        name="datatable",
        version=_get_version_from_build_info(),

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
            "Development Status :: 5 - Production/Stable",
            "Intended Audience :: Developers",
            "Intended Audience :: Science/Research",
            "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
            "Operating System :: MacOS",
            "Operating System :: Microsoft :: Windows",
            "Operating System :: Unix",
            "Programming Language :: Python :: 3 :: Only",
            "Programming Language :: Python :: 3.8",
            "Programming Language :: Python :: 3.9",
            "Programming Language :: Python :: 3.10",
            "Programming Language :: Python :: 3.11",
            "Topic :: Scientific/Engineering :: Information Analysis",
        ],

        # Runtime dependencies
        requirements=[
            "pytest (>=3.1); extra == 'tests'",
            "docutils (>=0.14); extra == 'tests'",
            "numpy; extra == 'optional'",
            "pandas; extra == 'optional'",
            "xlrd; extra == 'optional'",
        ],
        requires_python=">=3.6",
    )



#-------------------------------------------------------------------------------
# Build info file
#-------------------------------------------------------------------------------
def shell_cmd(
    cmd: typing.List[str], 
    strict: bool = False,
) -> str:
    try:
        process = subprocess.Popen(
            cmd, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE, 
            universal_newlines=True
        )
        stdout, stderr = process.communicate()
        
        if process.returncode != 0:
            # Command failed
            if strict:
                error_message = (
                    f"Command `{' '.join(cmd)}` failed with code "
                    f"{process.returncode}: {stderr}"
                )
                raise SystemExit(error_message)
            else:
                return ""

        # If the command succeeds, return stdout
        return stdout.strip()
    except subprocess.CalledProcessError as e:
        return ""
    except Exception as e:
        if strict:
            error_message = (
                f"Command `{' '.join(cmd)}` encountered an error: {str(e)}"
            )
            raise SystemExit(error_message)
            
        return ""


def generate_build_info(mode=None, strict=False):
    """
    Gather the build information and write it into the
    datatable/_build_info.py file.

    Parameters
    ----------
    mode: str
        Used only for local version tags, the mode is the first part
        of such local tag.

    strict: bool
        If False, then the errors in git commands will be silently
        ignored, and the produced _build_info.py file will contain
        empty `git_revision` and `git_branch` fields.

        If True, then the errors in git commands will terminate the
        build process.
    """
    version = get_datatable_version(mode)
    build_date = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())
    git_hash = shell_cmd(["git", "rev-parse", "HEAD"], strict=strict)
    # get the date of the commit (HEAD), as a Unix timestamp
    git_date = shell_cmd(["git", "show", "-s", "--format=%ct", "HEAD"],
                         strict=strict)
    if "CHANGE_BRANCH" in os.environ:
        git_branch = os.environ["CHANGE_BRANCH"]
    elif "APPVEYOR_REPO_BRANCH" in os.environ:
        git_branch = os.environ["APPVEYOR_REPO_BRANCH"]
    else:
        git_branch = shell_cmd(["git", "rev-parse", "--abbrev-ref", "HEAD"],
                               strict=strict)
    git_diff = shell_cmd(["git", "diff", "HEAD", "--stat", "--no-color"],
                         strict=strict)
    
    # Reformat the `git_date` as a UTC time string
    if git_date:
        git_date = time.strftime("%Y-%m-%d %H:%M:%S",
                                 time.gmtime(int(git_date)))
    if mode == 'build':
        mode = 'release'

    info_file = os.path.join("src", "datatable", "_build_info.py")
    with open(info_file, "wt") as out:
        out.write(
            "#!/usr/bin/env python3\n"
            "# -*- encoding: utf-8 -*-\n"
            "# --------------------------------------------------------------\n"
            "# Copyright 2018-%d H2O.ai\n"
            "#\n"
            "# Permission is hereby granted, free of charge, to any person\n"
            "# obtaining a copy of this software and associated documentation\n"
            "# files (the 'Software'), to deal in the Software without\n"
            "# restriction, including without limitation the rights to use,\n"
            "# copy, modify, merge, publish, distribute, sublicense, and/or\n"
            "# sell copies of the Software, and to permit persons to whom the\n"
            "# Software is furnished to do so, subject to the following\n"
            "# conditions:\n"
            "#\n"
            "# The above copyright notice and this permission notice shall be\n"
            "# included in all copies or substantial portions of the\n"
            "# Software.\n"
            "#\n"
            "# THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY\n"
            "# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE\n"
            "# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR\n"
            "# PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS\n"
            "# OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
            "# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR\n"
            "# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE\n"
            "# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n"
            "# --------------------------------------------------------------\n"
            "# This file was auto-generated from ci/ext.py\n\n"
            % time.gmtime().tm_year
        )
        out.write("import types\n\n")
        out.write("try:\n")
        out.write("    import datatable.lib._datatable as _dt\n")
        out.write("    _compiler = _dt._compiler()\n")
        out.write("except:\n")
        out.write("    _compiler = 'unknown'\n")
        out.write("\n")
        out.write("build_info = types.SimpleNamespace(\n")
        out.write("    version='%s',\n" % version)
        out.write("    build_date='%s',\n" % build_date)
        out.write("    build_mode='%s',\n" % mode)
        out.write("    compiler=_compiler,\n")
        out.write("    git_revision='%s',\n" % git_hash)
        out.write("    git_branch='%s',\n" % git_branch)
        out.write("    git_date='%s',\n" % git_date)
        if git_diff:
            lines = git_diff.split('\n')
            assert not any("'" in line for line in lines)
            out.write("    git_diff='%s" % lines[0].strip())
            for line in lines[1:]:
                out.write("\\n'\n             '%s" % line.strip())
            out.write("',\n")
        else:
            out.write("    git_diff='',\n")
        out.write(")\n")




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
    reuse_extension = config_settings.pop("reuse_extension", False)
    reuse_version = config_settings.pop("reuse_version", None)
    debug_wheel = config_settings.pop("debug", False)

    if is_source_distribution() and reuse_version is None:
        config_settings["reuse_version"] = True

    if not reuse_version:
        flavor = "custom" if reuse_extension else \
                 "debug" if debug_wheel else \
                 "build"
        generate_build_info(flavor, strict=not is_source_distribution())
    assert os.path.isfile("src/datatable/_build_info.py")

    if reuse_extension:
        pyver = "%d%d" % sys.version_info[:2]
        soext = "dll" if sys.platform == "win32" else "so"
        pattern = "src/datatable/lib/_datatable.cpython-%s*.%s" % (pyver, soext)
        sofiles = glob.glob(pattern)
        if not sofiles:
            raise SystemExit("Extension file %s not found" % pattern)
        if len(sofiles) > 1:
            raise SystemExit("Multiple extension files found: %r" % (sofiles,))
        so_file = sofiles[0]
    else:
        so_file = build_extension(cmd=("debug" if debug_wheel else "build"),
                                  verbosity=3)

    files = glob.glob("src/datatable/**/*.py", recursive=True)
    files += [so_file]
    files += ["src/datatable/include/datatable.h"]
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

    generate_build_info("sdist", strict=True)

    files = [f for f in glob.glob("src/datatable/**/*.py", recursive=True)
             if "_datatable_builder.py" not in f]
    files += glob.glob("src/core/**/*.cc", recursive=True)
    files += glob.glob("src/core/**/*.h", recursive=True)
    files += glob.glob("ci/xbuild/*.py")
    files += glob.glob("docs/**/*.rst", recursive=True)
    files += [f for f in glob.glob("tests/**/*.py", recursive=True)]
    files += [f for f in glob.glob("tests_random/*.py")]
    files += ["src/datatable/include/datatable.h"]
    files.sort()
    files += ["ci/ext.py", "ci/__init__.py", "ci/gendoc.py"]
    files += ["pyproject.toml"]
    files += ["LICENSE"]
    # See `is_source_distribution()`
    assert "VERSION.txt" not in files

    meta = get_meta()
    wb = xbuild.Wheel(files, **meta)
    wb.log = create_logger(verbosity=3)
    sdist_file = wb.build_sdist(sdist_directory)
    return sdist_file




#-------------------------------------------------------------------------------
# Allow this script to run from command line
#-------------------------------------------------------------------------------

def cmd_ext(args):
    with open("src/datatable/lib/.xbuild-cmd", "wt") as out:
        out.write(args.cmd)
    generate_build_info(args.cmd, strict=args.strict)
    build_extension(cmd=args.cmd, verbosity=args.verbosity)


def cmd_geninfo(args):
    generate_build_info(strict=args.strict)


def cmd_sdist(args):
    sdist_file = build_sdist(args.destination)
    assert os.path.isfile(os.path.join(args.destination, sdist_file))


def cmd_wheel(args):
    params = {
        "audit": args.audit,
        "debug": (args.cmd == "debugwheel"),
        "reuse_extension": args.nobuild,
    }
    wheel_file = build_wheel(args.destination, params)
    assert os.path.isfile(os.path.join(args.destination, wheel_file))



def main():
    import argparse
    parser = argparse.ArgumentParser(
        description='Build _datatable module',
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("cmd", metavar="CMD",
        choices=["asan", "build", "coverage", "debug", "geninfo", "sdist",
                 "wheel", "debugwheel"],
        help=textwrap.dedent("""
            Specify what this script should do:

            asan     : build _datatable with Address Sanitizer enabled
            build    : build _datatable normally, with full optimization
            coverage : build _datatable in a mode suitable for coverage
                       testing
            debug    : build _datatable in debug mode, optimized for gdb
                       on Linux and for lldb on MacOS
            geninfo  : generate _build_info.py file
            sdist    : create source distribution of datatable
            wheel    : create wheel distribution of datatable
            debugwheel : create wheel distribution of debug version of datatable
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
    parser.add_argument("--strict", action="store_true",
        help="This flag is used for `geninfo` command: when given, the\n"
             "generated _build_info.py file is guaranteed to contain the\n"
             "git_revision and git_branch fields, or otherwise an error\n"
             "will be thrown. This flag is turned on automatically for\n"
             "`sdist` and `wheel` commands.")
    parser.add_argument("--nobuild", action="store_true",
        help="This flag is used for `wheel` command: it indicates that\n"
             "the _datatable dynamic library should not be rebuilt.\n"
             "Instead, the library will be taken as-is from the lib/\n"
             "folder. The user is expected to have it pre-built manually.")

    args = parser.parse_args()
    if args.audit and "linux" not in sys.platform:
        raise ValueError("Argument --audit can be used on a Linux platform "
                         "only, current platform is `%s`" % sys.platform)

    if "wheel" in args.cmd:     cmd_wheel(args)
    elif args.cmd == "sdist":   cmd_sdist(args)
    elif args.cmd == "geninfo": cmd_geninfo(args)
    else:                       cmd_ext(args)


if __name__ == "__main__":
    main()
