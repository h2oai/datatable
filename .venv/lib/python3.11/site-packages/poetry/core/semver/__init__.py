from __future__ import annotations

import warnings


warnings.warn(
    "poetry.core.semver is deprecated. Use poetry.core.constraints.version instead.",
    DeprecationWarning,
    stacklevel=2,
)
