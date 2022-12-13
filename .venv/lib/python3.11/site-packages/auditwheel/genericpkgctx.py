from typing import Optional, Union

from .condatools import InCondaPkgCtx
from .wheeltools import InWheelCtx


def InGenericPkgCtx(
    in_path: str, out_path: Optional[str] = None
) -> Union[InWheelCtx, InCondaPkgCtx]:
    """Factory that returns a InWheelCtx or InCondaPkgCtx
    context manager depending on the file extension
    """
    if in_path.endswith(".whl"):
        return InWheelCtx(in_path, out_path)
    if in_path.endswith(".tar.bz2"):
        if out_path is not None:
            raise NotImplementedError()
        return InCondaPkgCtx(in_path)

    raise ValueError(
        "Invalid package: %s. File formats supported: " ".whl, .tar.bz2" % in_path
    )
