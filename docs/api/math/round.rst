
.. xfunction:: datatable.math.round
    :src: src/core/expr/fexpr_round.cc pyfn_round
    :tests: tests/math/test-round.py
    :cvar: doc_math_round
    :signature: round(cols, ndigits=None)

    .. x-version-added:: 0.11

    Round the values in `cols` up to the specified number of the digits
    of precision `ndigits`. If the number of digits is omitted, rounds
    to the nearest integer.

    Generally, this operation is equivalent to::

        >>> rint(col * 10**ndigits) / 10**ndigits

    where function `rint()` rounds to the nearest integer.

    Parameters
    ----------
    cols: FExpr
        Input data for rounding. This could be an expression yielding
        either a single or multiple columns. The `round()` function will
        apply to each column independently and produce as many columns
        in the output as there were in the input.

        Only numeric columns are allowed: boolean, integer or float.
        An exception will be raised if `cols` contains a non-numeric
        column.

    ndigits: int | None
        The number of precision digits to retain. This parameter could
        be either positive or negative (or None). If positive then it
        gives the number of digits after the decimal point. If negative,
        then it rounds the result up to the corresponding power of 10.

        For example, `123.45` rounded to `ndigits=1` is `123.4`, whereas
        rounded to `ndigits=-1` it becomes `120.0`.

    return: FExpr
        f-expression that rounds the values in its first argument to
        the specified number of precision digits.

        Each input column will produce the column of the same stype in
        the output; except for the case when `ndigits` is `None` and
        the input is either `float32` or `float64`, in which case an
        `int64` column is produced (similarly to python's `round()`).

    Notes
    -----
    Values that are exactly half way in between their rounded neighbors
    are converted towards their nearest even value. For example, both
    `7.5` and `8.5` are rounded into `8`, whereas `6.5` is rounded as `6`.

    Rounding integer columns may produce unexpected results for values
    that are close to the min/max value of that column's storage type.
    For example, when an `int8` value `127` is rounded to nearest `10`, it
    becomes `130`. However, since `130` cannot be represented as `int8` a
    wrap-around occurs and the result becomes `-126`.

    Rounding an integer column to a positive `ndigits` is a noop: the
    column will be returned unchanged.

    Rounding an integer column to a large negative `ndigits` will produce
    a constant all-0 column.
