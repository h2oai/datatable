
.. xfunction:: datatable.ifelse
    :src: src/core/expr/fexpr_ifelse.cc FExpr_IfElse::evaluate_n
    :cvar: doc_dt_ifelse
    :tests: tests/dt/test-ifelse.py
    :signature: ifelse(condition1, value1, condition2, value2, ..., default)

    .. x-version-added:: 0.11.0

    An expression that chooses its value based on one or more
    conditions.

    This is roughly equivalent to the following Python code::

        >>> result = value1 if condition1 else \
        ...          value2 if condition2 else \
        ...          ...                  else \
        ...          default

    For every row this function evaluates the smallest number of expressions
    necessary to get the result. Thus, it evaluates `condition1`, `condition2`,
    and so on until it finds the first condition that evaluates to `True`.
    It then computes and returns the corresponding `value`. If all conditions
    evaluate to `False`, then the `default` value is computed and returned.

    Also, if any of the conditions produces NA then the result of the expression
    also becomes NA without evaluating any further conditions or values.


    Parameters
    ----------
    condition1, condition2, ...: FExpr[bool]
        Expressions each producing a single boolean column. These conditions
        will be evaluated in order until we find the one equal to `True`.

    value1, value2, ...: FExpr
        Values that will be used when the corresponding condition evaluates
        to `True`. These must be single columns.

    default: FExpr
        Value that will be used when all conditions evaluate to `False`.
        This must be a single column.

    return: FExpr
        The resulting expression is a single column whose stype is the
        common stype for all `value1`, ..., `default` columns.


    Notes
    -----
    .. x-version-changed:: 1.0.0

        Earlier this function accepted a single condition only.

    Examples
    --------

    Single condition
    ~~~~~~~~~~~~~~~~
    Task: Create a new column `Colour`, where if `Set` is `'Z'` then the
    value should be `'Green'`, else `'Red'`::

        >>> from datatable import dt, f, by, ifelse, update
        >>>
        >>> df = dt.Frame("""Type       Set
        ...                   A          Z
        ...                   B          Z
        ...                   B          X
        ...                   C          Y""")
        >>> df[:, update(Colour = ifelse(f.Set == "Z",  # condition
        ...                              "Green",       # if condition is True
        ...                              "Red"))        # if condition is False
        ... ]
        >>> df
           | Type   Set    Colour
           | str32  str32  str32
        -- + -----  -----  ------
         0 | A      Z      Green
         1 | B      Z      Green
         2 | B      X      Red
         3 | C      Y      Red
        [4 rows x 3 columns]


    Multiple conditions
    ~~~~~~~~~~~~~~~~~~~
    Task: Create new column ``value`` whose value is taken from columns ``a``,
    ``b``, or ``c`` -- whichever is nonzero first::

        >>> df = dt.Frame({"a": [0,0,1,2],
        ...                "b": [0,3,4,5],
        ...                "c": [6,7,8,9]})
        >>> df
           |     a      b      c
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     0      0      6
         1 |     0      3      7
         2 |     1      4      8
         3 |     2      5      9
        [4 rows x 3 columns]

        >>> df['value'] = ifelse(f.a > 0, f.a,  # first condition and result
        ...                      f.b > 0, f.b,  # second condition and result
        ...                      f.c)           # default if no condition is True
        >>> df
           |     a      b      c  value
           | int32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 |     0      0      6      6
         1 |     0      3      7      3
         2 |     1      4      8      1
         3 |     2      5      9      2
        [4 rows x 4 columns]

