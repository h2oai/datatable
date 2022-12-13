import itertools
import logging
import os
import platform
import re
import shutil
import stat
from collections import OrderedDict
from os.path import abspath, basename, dirname, exists, isabs
from os.path import join as pjoin
from subprocess import check_call
from typing import Dict, Iterable, List, Optional, Tuple

from auditwheel.patcher import ElfPatcher

from .elfutils import elf_read_dt_needed, elf_read_rpaths, is_subdir
from .hashfile import hashfile
from .policy import get_replace_platforms
from .wheel_abi import get_wheel_elfdata
from .wheeltools import InWheelCtx, add_platforms

logger = logging.getLogger(__name__)


# Copied from wheel 0.31.1
WHEEL_INFO_RE = re.compile(
    r"""^(?P<namever>(?P<name>.+?)-(?P<ver>\d.*?))(-(?P<build>\d.*?))?
     -(?P<pyver>[a-z].+?)-(?P<abi>.+?)-(?P<plat>.+?)(\.whl|\.dist-info)$""",
    re.VERBOSE,
).match


def repair_wheel(
    wheel_path: str,
    abis: List[str],
    lib_sdir: str,
    out_dir: str,
    update_tags: bool,
    patcher: ElfPatcher,
    exclude: List[str],
    strip: bool = False,
) -> Optional[str]:

    external_refs_by_fn = get_wheel_elfdata(wheel_path)[1]

    # Do not repair a pure wheel, i.e. has no external refs
    if not external_refs_by_fn:
        return None

    soname_map = {}  # type: Dict[str, Tuple[str, str]]
    if not isabs(out_dir):
        out_dir = abspath(out_dir)

    wheel_fname = basename(wheel_path)

    with InWheelCtx(wheel_path) as ctx:
        ctx.out_wheel = pjoin(out_dir, wheel_fname)

        match = WHEEL_INFO_RE(wheel_fname)
        if not match:
            raise ValueError("Failed to parse wheel file name: %s", wheel_fname)

        dest_dir = match.group("name") + lib_sdir

        if not exists(dest_dir):
            os.mkdir(dest_dir)

        # here, fn is a path to a python extension library in
        # the wheel, and v['libs'] contains its required libs
        for fn, v in external_refs_by_fn.items():
            ext_libs = v[abis[0]]["libs"]  # type: Dict[str, str]
            replacements = []  # type: List[Tuple[str, str]]
            for soname, src_path in ext_libs.items():

                if soname in exclude:
                    logger.info(f"Excluding {soname}")
                    continue

                if src_path is None:
                    raise ValueError(
                        (
                            "Cannot repair wheel, because required "
                            'library "%s" could not be located'
                        )
                        % soname
                    )

                new_soname, new_path = copylib(src_path, dest_dir, patcher)
                soname_map[soname] = (new_soname, new_path)
                replacements.append((soname, new_soname))
            if replacements:
                patcher.replace_needed(fn, *replacements)

            if len(ext_libs) > 0:
                new_rpath = os.path.relpath(dest_dir, os.path.dirname(fn))
                new_rpath = os.path.join("$ORIGIN", new_rpath)
                append_rpath_within_wheel(fn, new_rpath, ctx.name, patcher)

        # we grafted in a bunch of libraries and modified their sonames, but
        # they may have internal dependencies (DT_NEEDED) on one another, so
        # we need to update those records so each now knows about the new
        # name of the other.
        for old_soname, (new_soname, path) in soname_map.items():
            needed = elf_read_dt_needed(path)
            replacements = []
            for n in needed:
                if n in soname_map:
                    replacements.append((n, soname_map[n][0]))
            if replacements:
                patcher.replace_needed(path, *replacements)

        if update_tags:
            ctx.out_wheel = add_platforms(ctx, abis, get_replace_platforms(abis[0]))

        if strip:
            libs_to_strip = [path for (_, path) in soname_map.values()]
            extensions = external_refs_by_fn.keys()
            strip_symbols(itertools.chain(libs_to_strip, extensions))

    return ctx.out_wheel


def strip_symbols(libraries: Iterable[str]) -> None:
    for lib in libraries:
        logger.info("Stripping symbols from %s", lib)
        check_call(["strip", "-s", lib])


def copylib(src_path: str, dest_dir: str, patcher: ElfPatcher) -> Tuple[str, str]:
    """Graft a shared library from the system into the wheel and update the
    relevant links.

    1) Copy the file from src_path to dest_dir/
    2) Rename the shared object from soname to soname.<unique>
    3) If the library has a RUNPATH/RPATH, clear it and set RPATH to point to
    its new location.
    """
    # Copy the a shared library from the system (src_path) into the wheel
    # if the library has a RUNPATH/RPATH we clear it and set RPATH to point to
    # its new location.

    with open(src_path, "rb") as f:
        shorthash = hashfile(f)[:8]

    src_name = os.path.basename(src_path)
    base, ext = src_name.split(".", 1)
    if not base.endswith("-%s" % shorthash):
        new_soname = f"{base}-{shorthash}.{ext}"
    else:
        new_soname = src_name

    dest_path = os.path.join(dest_dir, new_soname)
    if os.path.exists(dest_path):
        return new_soname, dest_path

    logger.debug("Grafting: %s -> %s", src_path, dest_path)
    rpaths = elf_read_rpaths(src_path)
    shutil.copy2(src_path, dest_path)
    statinfo = os.stat(dest_path)
    if not statinfo.st_mode & stat.S_IWRITE:
        os.chmod(dest_path, statinfo.st_mode | stat.S_IWRITE)

    patcher.set_soname(dest_path, new_soname)

    if any(itertools.chain(rpaths["rpaths"], rpaths["runpaths"])):
        patcher.set_rpath(dest_path, dest_dir)

    return new_soname, dest_path


def append_rpath_within_wheel(
    lib_name: str, rpath: str, wheel_base_dir: str, patcher: ElfPatcher
) -> None:
    """Add a new rpath entry to a file while preserving as many existing
    rpath entries as possible.

    In order to preserve an rpath entry it must:

    1) Point to a location within wheel_base_dir.
    2) Not be a duplicate of an already-existing rpath entry.
    """
    if not isabs(lib_name):
        lib_name = abspath(lib_name)
    lib_dir = dirname(lib_name)
    if not isabs(wheel_base_dir):
        wheel_base_dir = abspath(wheel_base_dir)

    def is_valid_rpath(rpath: str) -> bool:
        return _is_valid_rpath(rpath, lib_dir, wheel_base_dir)

    old_rpaths = patcher.get_rpath(lib_name)
    rpaths = filter(is_valid_rpath, old_rpaths.split(":"))
    # Remove duplicates while preserving ordering
    # Fake an OrderedSet using OrderedDict
    rpath_set = OrderedDict([(old_rpath, "") for old_rpath in rpaths])
    rpath_set[rpath] = ""

    patcher.set_rpath(lib_name, ":".join(rpath_set))


def _is_valid_rpath(rpath: str, lib_dir: str, wheel_base_dir: str) -> bool:
    full_rpath_entry = _resolve_rpath_tokens(rpath, lib_dir)
    if not isabs(full_rpath_entry):
        logger.debug(
            f"rpath entry {rpath} could not be resolved to an "
            "absolute path -- discarding it."
        )
        return False
    elif not is_subdir(full_rpath_entry, wheel_base_dir):
        logger.debug(
            f"rpath entry {rpath} points outside the wheel -- " "discarding it."
        )
        return False
    else:
        logger.debug(f"Preserved rpath entry {rpath}")
        return True


def _resolve_rpath_tokens(rpath: str, lib_base_dir: str) -> str:
    # See https://www.man7.org/linux/man-pages/man8/ld.so.8.html#DESCRIPTION
    if platform.architecture()[0] == "64bit":
        system_lib_dir = "lib64"
    else:
        system_lib_dir = "lib"
    system_processor_type = platform.machine()
    token_replacements = {
        "ORIGIN": lib_base_dir,
        "LIB": system_lib_dir,
        "PLATFORM": system_processor_type,
    }
    for token, target in token_replacements.items():
        rpath = rpath.replace(f"${token}", target)  # $TOKEN
        rpath = rpath.replace(f"${{{token}}}", target)  # ${TOKEN}
    return rpath
