
.. xmethod:: datatable.FExpr.__add__
    :src: src/core/expr/fbinary/fexpr__add__.cc evaluate1
    :tests: tests/expr/fbinary/test-add.py

    __add__(x, y)
    --

    Add two FExprs together, which corresponds to python operator `+`.

    If `x` or `y` are multi-column expressions, then they must have the
    same number of columns, and the `+` operator will be applied to each
    corresponding pair of columns. If either `x` or `y` are single-column
    while the other is multi-column, then the single-column expression
    will be repeated to the same number of columns as its opponent.

    The result of adding two columns with different stypes will have the
    following stype:

      - `max(x.stype, y.stype, int32)` if both columns are numeric (i.e.
        bool, int or float);

      - `str32`/`str64` if at least one of the columns is a string. In
        this case the `+` operator implements string concatenation, same
        as in Python.

    Parameters
    ----------
    x, y: FExpr
        The arguments must be either `FExpr`s, or expressions that can be
        converted into `FExpr`s.

    return: FExpr
        An expression that evaluates `x + y`.
