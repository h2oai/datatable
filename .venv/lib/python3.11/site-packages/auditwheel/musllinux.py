import logging
import pathlib
import re
import subprocess
from typing import NamedTuple

from auditwheel.error import InvalidLibc

LOG = logging.getLogger(__name__)


class MuslVersion(NamedTuple):
    major: int
    minor: int
    patch: int


def find_musl_libc() -> pathlib.Path:
    try:
        (dl_path,) = list(pathlib.Path("/lib").glob("libc.musl-*.so.1"))
    except ValueError:
        LOG.debug("musl libc not detected")
        raise InvalidLibc

    return dl_path


def get_musl_version(ld_path: pathlib.Path) -> MuslVersion:
    try:
        ld = subprocess.run(
            [ld_path], check=False, errors="strict", stderr=subprocess.PIPE
        ).stderr
    except FileNotFoundError:
        LOG.error("Failed to determine musl version", exc_info=True)
        raise InvalidLibc

    match = re.search(
        r"Version " r"(?P<major>\d+)." r"(?P<minor>\d+)." r"(?P<patch>\d+)", ld
    )
    if not match:
        raise InvalidLibc

    return MuslVersion(
        int(match.group("major")), int(match.group("minor")), int(match.group("patch"))
    )
