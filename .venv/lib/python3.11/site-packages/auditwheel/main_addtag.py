import logging
from os.path import abspath, basename, exists, join

logger = logging.getLogger(__name__)


def configure_parser(sub_parsers):
    help = "Add new platform ABI tags to a wheel"

    p = sub_parsers.add_parser("addtag", help=help, description=help)
    p.add_argument("WHEEL_FILE", help="Path to wheel file")
    p.add_argument(
        "-w",
        "--wheel-dir",
        dest="WHEEL_DIR",
        help=("Directory to store new wheel file (default:" ' "wheelhouse/")'),
        type=abspath,
        default="wheelhouse/",
    )
    p.set_defaults(func=execute)


def execute(args, p):
    import os

    from ._vendor.wheel.wheelfile import WHEEL_INFO_RE
    from .wheel_abi import NonPlatformWheel, analyze_wheel_abi
    from .wheeltools import InWheelCtx, WheelToolsError, add_platforms

    try:
        wheel_abi = analyze_wheel_abi(args.WHEEL_FILE)
    except NonPlatformWheel:
        logger.info(NonPlatformWheel.LOG_MESSAGE)
        return 1

    parsed_fname = WHEEL_INFO_RE.search(basename(args.WHEEL_FILE))
    in_fname_tags = parsed_fname.groupdict()["plat"].split(".")

    logger.info(
        '%s receives the following tag: "%s".',
        basename(args.WHEEL_FILE),
        wheel_abi.overall_tag,
    )
    logger.info("Use ``auditwheel show`` for more details")

    if wheel_abi.overall_tag in in_fname_tags:
        logger.info("No tags to be added. Exiting.")
        return 1

    # todo: move more of this logic to separate file
    if not exists(args.WHEEL_DIR):
        os.makedirs(args.WHEEL_DIR)

    with InWheelCtx(args.WHEEL_FILE) as ctx:
        try:
            out_wheel = add_platforms(ctx, [wheel_abi.overall_tag])
        except WheelToolsError as e:
            logger.exception("\n%s.", repr(e))
            return 1

        if out_wheel:
            # tell context manager to write wheel on exit with
            # the proper output directory
            ctx.out_wheel = join(args.WHEEL_DIR, basename(out_wheel))
            logger.info("\nUpdated wheel written to %s", out_wheel)
    return 0
