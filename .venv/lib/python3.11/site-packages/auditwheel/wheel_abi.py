import functools
import itertools
import json
import logging
import os
from collections import defaultdict, namedtuple
from collections.abc import Mapping
from copy import deepcopy
from os.path import basename
from typing import Dict, Set

from .elfutils import (
    elf_file_filter,
    elf_find_ucs2_symbols,
    elf_find_versioned_symbols,
    elf_is_python_extension,
    elf_references_PyFPE_jbuf,
)
from .genericpkgctx import InGenericPkgCtx
from .lddtree import lddtree
from .policy import (
    POLICY_PRIORITY_HIGHEST,
    POLICY_PRIORITY_LOWEST,
    get_policy_name,
    lddtree_external_references,
    load_policies,
    versioned_symbols_policy,
)

log = logging.getLogger(__name__)
WheelAbIInfo = namedtuple(
    "WheelAbIInfo",
    [
        "overall_tag",
        "external_refs",
        "ref_tag",
        "versioned_symbols",
        "sym_tag",
        "ucs_tag",
        "pyfpe_tag",
        "blacklist_tag",
    ],
)


class WheelAbiError(Exception):
    """Root exception class"""


class NonPlatformWheel(WheelAbiError):
    """No ELF binaries in the wheel"""

    LOG_MESSAGE = (
        "This does not look like a platform wheel, no ELF executable "
        "or shared library file (including compiled Python C extension) "
        "found in the wheel archive"
    )


@functools.lru_cache()
def get_wheel_elfdata(wheel_fn: str):
    full_elftree = {}
    nonpy_elftree = {}
    full_external_refs = {}
    versioned_symbols = defaultdict(lambda: set())  # type: Dict[str, Set[str]]
    uses_ucs2_symbols = False
    uses_PyFPE_jbuf = False

    with InGenericPkgCtx(wheel_fn) as ctx:
        shared_libraries_in_purelib = []

        platform_wheel = False
        for fn, elf in elf_file_filter(ctx.iter_files()):
            platform_wheel = True

            # Check for invalid binary wheel format: no shared library should
            # be found in purelib
            so_path_split = fn.split(os.sep)

            # If this is in purelib, add it to the list of shared libraries in
            # purelib
            if "purelib" in so_path_split:
                shared_libraries_in_purelib.append(so_path_split[-1])

            # If at least one shared library exists in purelib, this is going
            # to fail and there's no need to do further checks
            if not shared_libraries_in_purelib:
                log.debug("processing: %s", fn)
                elftree = lddtree(fn)

                for key, value in elf_find_versioned_symbols(elf):
                    log.debug("key %s, value %s", key, value)
                    versioned_symbols[key].add(value)

                is_py_ext, py_ver = elf_is_python_extension(fn, elf)

                # If the ELF is a Python extention, we definitely need to
                # include its external dependencies.
                if is_py_ext:
                    full_elftree[fn] = elftree
                    uses_PyFPE_jbuf |= elf_references_PyFPE_jbuf(elf)
                    if py_ver == 2:
                        uses_ucs2_symbols |= any(
                            True for _ in elf_find_ucs2_symbols(elf)
                        )
                    full_external_refs[fn] = lddtree_external_references(
                        elftree, ctx.path
                    )
                else:
                    # If the ELF is not a Python extension, it might be
                    # included in the wheel already because auditwheel repair
                    # vendored it, so we will check whether we should include
                    # its internal references later.
                    nonpy_elftree[fn] = elftree

        if not platform_wheel:
            raise NonPlatformWheel

        # If at least one shared library exists in purelib, raise an error
        if shared_libraries_in_purelib:
            raise RuntimeError(
                (
                    "Invalid binary wheel, found the following shared "
                    "library/libraries in purelib folder:\n"
                    "\t%s\n"
                    "The wheel has to be platlib compliant in order to be "
                    "repaired by auditwheel."
                )
                % "\n\t".join(shared_libraries_in_purelib)
            )

        # Get a list of all external libraries needed by ELFs in the wheel.
        needed_libs = {
            lib
            for elf in itertools.chain(full_elftree.values(), nonpy_elftree.values())
            for lib in elf["needed"]
        }

        for fn in nonpy_elftree.keys():
            # If a non-pyextension ELF file is not needed by something else
            # inside the wheel, then it was not checked by the logic above and
            # we should walk its elftree.
            if basename(fn) not in needed_libs:
                full_elftree[fn] = nonpy_elftree[fn]

            # Even if a non-pyextension ELF file is not needed, we
            # should include it as an external reference, because
            # it might require additional external libraries.
            full_external_refs[fn] = lddtree_external_references(
                nonpy_elftree[fn], ctx.path
            )

    log.debug("full_elftree:\n%s", json.dumps(full_elftree, indent=4))
    log.debug(
        "full_external_refs (will be repaired):\n%s",
        json.dumps(full_external_refs, indent=4),
    )

    return (
        full_elftree,
        full_external_refs,
        versioned_symbols,
        uses_ucs2_symbols,
        uses_PyFPE_jbuf,
    )


def get_external_libs(external_refs) -> Dict[str, str]:
    """Get external library dependencies for all policies excluding the default
    linux policy
    :param external_refs: external references for all policies
    :return: {realpath: soname} e.g.
    {'/path/to/external_ref.so.1.2.3': 'external_ref.so.1'}
    """
    result: Dict[str, str] = {}
    for policy in external_refs.values():
        # linux tag (priority 0) has no white-list, do not analyze it
        if policy["priority"] == 0:
            continue
        # go through all libs, retrieving their soname and realpath
        for libname, realpath in policy["libs"].items():
            if realpath and realpath not in result.keys():
                result[realpath] = libname
    return result


def get_versioned_symbols(libs):
    """Get versioned symbols used in libraries
    :param libs: {realpath: soname} dict to search for versioned symbols e.g.
    {'/path/to/external_ref.so.1.2.3': 'external_ref.so.1'}
    :return: {soname: {depname: set([symbol_version])}} e.g.
    {'external_ref.so.1': {'libc.so.6', set(['GLIBC_2.5','GLIBC_2.12'])}}
    """
    result = {}
    for path, elf in elf_file_filter(libs.keys()):
        # {depname: set(symbol_version)}, e.g.
        # {'libc.so.6', set(['GLIBC_2.5','GLIBC_2.12'])}
        elf_versioned_symbols = defaultdict(lambda: set())
        for key, value in elf_find_versioned_symbols(elf):
            log.debug("path %s, key %s, value %s", path, key, value)
            elf_versioned_symbols[key].add(value)
        result[libs[path]] = elf_versioned_symbols
    return result


def get_symbol_policies(versioned_symbols, external_versioned_symbols, external_refs):
    """Get symbol policies
    Since white-list is different per policy, this function inspects
    versioned_symbol per policy when including external refs
    :param versioned_symbols: versioned symbols for the current wheel
    :param external_versioned_symbols: versioned symbols for external libs
    :param external_refs: external references for all policies
    :return: list of tuples of the form (policy_priority, versioned_symbols),
    e.g. [(100, {'libc.so.6', set(['GLIBC_2.5'])})]
    """
    result = []
    for policy in external_refs.values():
        # skip the linux policy
        if policy["priority"] == 0:
            continue
        policy_symbols = deepcopy(versioned_symbols)
        for soname in policy["libs"].keys():
            if soname not in external_versioned_symbols:
                continue
            ext_symbols = external_versioned_symbols[soname]
            for k in iter(ext_symbols):
                policy_symbols[k].update(ext_symbols[k])
        result.append((versioned_symbols_policy(policy_symbols), policy_symbols))
    return result


def analyze_wheel_abi(wheel_fn: str) -> WheelAbIInfo:
    external_refs = {
        p["name"]: {"libs": {}, "blacklist": {}, "priority": p["priority"]}
        for p in load_policies()
    }

    (
        elftree_by_fn,
        external_refs_by_fn,
        versioned_symbols,
        has_ucs2,
        uses_PyFPE_jbuf,
    ) = get_wheel_elfdata(wheel_fn)

    for fn in elftree_by_fn.keys():
        update(external_refs, external_refs_by_fn[fn])

    log.debug("external reference info")
    log.debug(json.dumps(external_refs, indent=4))

    external_libs = get_external_libs(external_refs)
    external_versioned_symbols = get_versioned_symbols(external_libs)
    symbol_policies = get_symbol_policies(
        versioned_symbols, external_versioned_symbols, external_refs
    )
    symbol_policy = versioned_symbols_policy(versioned_symbols)

    # let's keep the highest priority policy and
    # corresponding versioned_symbols
    symbol_policy, versioned_symbols = max(
        symbol_policies, key=lambda x: x[0], default=(symbol_policy, versioned_symbols)
    )

    ref_policy = max(
        (e["priority"] for e in external_refs.values() if len(e["libs"]) == 0),
        default=POLICY_PRIORITY_LOWEST,
    )

    blacklist_policy = max(
        (e["priority"] for e in external_refs.values() if len(e["blacklist"]) == 0),
        default=POLICY_PRIORITY_LOWEST,
    )

    if has_ucs2:
        ucs_policy = POLICY_PRIORITY_LOWEST
    else:
        ucs_policy = POLICY_PRIORITY_HIGHEST

    if uses_PyFPE_jbuf:
        pyfpe_policy = POLICY_PRIORITY_LOWEST
    else:
        pyfpe_policy = POLICY_PRIORITY_HIGHEST

    ref_tag = get_policy_name(ref_policy)
    sym_tag = get_policy_name(symbol_policy)
    ucs_tag = get_policy_name(ucs_policy)
    pyfpe_tag = get_policy_name(pyfpe_policy)
    blacklist_tag = get_policy_name(blacklist_policy)
    overall_tag = get_policy_name(
        min(symbol_policy, ref_policy, ucs_policy, pyfpe_policy, blacklist_policy)
    )

    return WheelAbIInfo(
        overall_tag,
        external_refs,
        ref_tag,
        versioned_symbols,
        sym_tag,
        ucs_tag,
        pyfpe_tag,
        blacklist_tag,
    )


def update(d, u):
    for k, v in u.items():
        if k == "blacklist":
            for lib, symbols in v.items():
                if lib not in d[k]:
                    d[k][lib] = list(symbols)
                else:
                    d[k][lib] = sorted(set(d[k][lib]) | set(symbols))
        elif isinstance(v, Mapping):
            r = update(d.get(k, {}), v)
            d[k] = r
        elif isinstance(v, (str, int, float, type(None))):
            d[k] = u[k]
        else:
            raise RuntimeError("!", d, k)
    return d
