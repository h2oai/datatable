import re
from distutils.spawn import find_executable
from itertools import chain
from subprocess import CalledProcessError, check_call, check_output
from typing import Tuple


class ElfPatcher:
    def replace_needed(self, file_name: str, *old_new_pairs: Tuple[str, str]) -> None:
        raise NotImplementedError

    def set_soname(self, file_name: str, new_so_name: str) -> None:
        raise NotImplementedError

    def set_rpath(self, file_name: str, rpath: str) -> None:
        raise NotImplementedError

    def get_rpath(self, file_name: str) -> str:
        raise NotImplementedError


def _verify_patchelf() -> None:
    """This function looks for the ``patchelf`` external binary in the PATH,
    checks for the required version, and throws an exception if a proper
    version can't be found. Otherwise, silcence is golden
    """
    if not find_executable("patchelf"):
        raise ValueError("Cannot find required utility `patchelf` in PATH")
    try:
        version = check_output(["patchelf", "--version"]).decode("utf-8")
    except CalledProcessError:
        raise ValueError("Could not call `patchelf` binary")

    m = re.match(r"patchelf\s+(\d+(.\d+)?)", version)
    if m and tuple(int(x) for x in m.group(1).split(".")) >= (0, 14):
        return
    raise ValueError(
        f"patchelf {version} found. auditwheel repair requires " "patchelf >= 0.14."
    )


class Patchelf(ElfPatcher):
    def __init__(self) -> None:
        _verify_patchelf()

    def replace_needed(self, file_name: str, *old_new_pairs: Tuple[str, str]) -> None:
        check_call(
            [
                "patchelf",
                *chain.from_iterable(
                    ("--replace-needed", *pair) for pair in old_new_pairs
                ),
                file_name,
            ]
        )

    def set_soname(self, file_name: str, new_so_name: str) -> None:
        check_call(["patchelf", "--set-soname", new_so_name, file_name])

    def set_rpath(self, file_name: str, rpath: str) -> None:

        check_call(["patchelf", "--remove-rpath", file_name])
        check_call(["patchelf", "--force-rpath", "--set-rpath", rpath, file_name])

    def get_rpath(self, file_name: str) -> str:

        return (
            check_output(["patchelf", "--print-rpath", file_name])
            .decode("utf-8")
            .strip()
        )
