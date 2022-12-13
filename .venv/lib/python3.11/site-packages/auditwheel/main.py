import argparse
import logging
import os
import pathlib
import sys

if sys.version_info[:2] >= (3, 8):
    from importlib import metadata
else:
    import importlib_metadata as metadata

from typing import Optional

import auditwheel

from . import main_addtag, main_lddtree, main_repair, main_show


def main() -> Optional[int]:
    if sys.platform != "linux":
        print("Error: This tool only supports Linux")
        return 1

    location = pathlib.Path(auditwheel.__file__).parent.resolve()
    version = "auditwheel {} installed at {} (python {}.{})".format(
        metadata.version("auditwheel"), location, *sys.version_info
    )

    p = argparse.ArgumentParser(description="Cross-distro Python wheels.")
    p.set_defaults(prog=os.path.basename(sys.argv[0]))
    p.add_argument("-V", "--version", action="version", version=version)
    p.add_argument(
        "-v",
        "--verbose",
        action="count",
        dest="verbose",
        default=0,
        help="Give more output. Option is additive",
    )
    sub_parsers = p.add_subparsers(metavar="command", dest="cmd")

    main_show.configure_parser(sub_parsers)
    main_addtag.configure_parser(sub_parsers)
    main_repair.configure_parser(sub_parsers)
    main_lddtree.configure_subparser(sub_parsers)

    args = p.parse_args()

    logging.disable(logging.NOTSET)
    if args.verbose >= 1:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)

    if not hasattr(args, "func"):
        p.print_help()
        return None

    rval = args.func(args, p)

    return rval
