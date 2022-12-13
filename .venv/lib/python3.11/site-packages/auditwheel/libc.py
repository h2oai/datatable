import logging
from enum import IntEnum

from .error import InvalidLibc
from .musllinux import find_musl_libc

logger = logging.getLogger(__name__)


class Libc(IntEnum):
    GLIBC = (1,)
    MUSL = (2,)


def get_libc() -> Libc:
    try:
        find_musl_libc()
        logger.debug("Detected musl libc")
        return Libc.MUSL
    except InvalidLibc:
        logger.debug("Falling back to GNU libc")
        return Libc.GLIBC
