import logging
from collections import OrderedDict

logger = logging.getLogger(__name__)


def configure_parser(sub_parsers):
    help = "Audit a wheel for external shared library dependencies."
    p = sub_parsers.add_parser("show", help=help, description=help)
    p.add_argument("WHEEL_FILE", help="Path to wheel file.")
    p.set_defaults(func=execute)


def printp(text: str) -> None:
    from textwrap import wrap

    print()
    print("\n".join(wrap(text, break_long_words=False, break_on_hyphens=False)))


def execute(args, p):
    import json
    from os.path import basename, isfile

    from .policy import (
        POLICY_PRIORITY_HIGHEST,
        POLICY_PRIORITY_LOWEST,
        get_policy_name,
        get_priority_by_name,
        load_policies,
    )
    from .wheel_abi import NonPlatformWheel, analyze_wheel_abi

    fn = basename(args.WHEEL_FILE)

    if not isfile(args.WHEEL_FILE):
        p.error("cannot access %s. No such file" % args.WHEEL_FILE)

    try:
        winfo = analyze_wheel_abi(args.WHEEL_FILE)
    except NonPlatformWheel:
        logger.info(NonPlatformWheel.LOG_MESSAGE)
        return 1

    libs_with_versions = [
        f"{k} with versions {v}" for k, v in winfo.versioned_symbols.items()
    ]

    printp(
        '%s is consistent with the following platform tag: "%s".'
        % (fn, winfo.overall_tag)
    )

    if get_priority_by_name(winfo.pyfpe_tag) < POLICY_PRIORITY_HIGHEST:
        printp(
            "This wheel uses the PyFPE_jbuf function, which is not compatible with the"
            " manylinux1 tag. (see https://www.python.org/dev/peps/pep-0513/"
            "#fpectl-builds-vs-no-fpectl-builds)"
        )
        if args.verbose < 1:
            return

    if get_priority_by_name(winfo.ucs_tag) < POLICY_PRIORITY_HIGHEST:
        printp(
            "This wheel is compiled against a narrow unicode (UCS2) "
            "version of Python, which is not compatible with the "
            "manylinux1 tag."
        )
        if args.verbose < 1:
            return

    if len(libs_with_versions) == 0:
        printp(
            "The wheel references no external versioned symbols from "
            "system-provided shared libraries."
        )
    else:
        printp(
            "The wheel references external versioned symbols in these "
            "system-provided shared libraries: %s" % ", ".join(libs_with_versions)
        )

    if get_priority_by_name(winfo.sym_tag) < POLICY_PRIORITY_HIGHEST:
        printp(
            (
                'This constrains the platform tag to "%s". '
                "In order to achieve a more compatible tag, you would "
                "need to recompile a new wheel from source on a system "
                "with earlier versions of these libraries, such as "
                "a recent manylinux image."
            )
            % winfo.sym_tag
        )
        if args.verbose < 1:
            return

    libs = winfo.external_refs[get_policy_name(POLICY_PRIORITY_LOWEST)]["libs"]
    if len(libs) == 0:
        printp("The wheel requires no external shared libraries! :)")
    else:
        printp("The following external shared libraries are required " "by the wheel:")
        print(json.dumps(OrderedDict(sorted(libs.items())), indent=4))

    for p in sorted(load_policies(), key=lambda p: p["priority"]):
        if p["priority"] > get_priority_by_name(winfo.overall_tag):
            libs = winfo.external_refs[p["name"]]["libs"]
            if len(libs):
                printp(
                    (
                        'In order to achieve the tag platform tag "%s" '
                        "the following shared library dependencies "
                        "will need to be eliminated:"
                    )
                    % p["name"]
                )
                printp(", ".join(sorted(libs.keys())))
            blacklist = winfo.external_refs[p["name"]]["blacklist"]
            if len(blacklist):
                printp(
                    (
                        'In order to achieve the tag platform tag "%s" '
                        "the following black-listed symbol dependencies "
                        "will need to be eliminated:"
                    )
                    % p["name"]
                )
                for key in sorted(blacklist.keys()):
                    printp(f"From {key}: " + ", ".join(sorted(blacklist[key])))
