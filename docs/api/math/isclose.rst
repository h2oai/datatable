
.. xfunction:: datatable.math.isclose
    :src: src/core/expr/head_func_isclose.cc pyfn_isclose
    :cvar: doc_math_isclose
    :signature: isclose(x, y, *, rtol=1e-5, atol=1e-8)

    Compare two numbers x and y, and return True if they are close
    within the requested relative/absolute tolerance. This function
    only returns True/False, never NA.

    More specifically, isclose(x, y) is True if either of the following
    are true:

    - ``x == y`` (including the case when x and y are NAs),
    - ``abs(x - y) <= atol + rtol * abs(y)`` and neither x nor y are NA

    The tolerance parameters ``rtol``, ``atol`` must be positive floats,
    and cannot be expressions.
