
.. xfunction:: datatable.cov
    :src: src/core/expr/head_reduce_binary.cc compute_cov
    :tests: tests/test-reduce.py
    :cvar: doc_dt_cov
    :signature: cov(col1, col2)

    Calculate `covariance <https://en.wikipedia.org/wiki/Covariance>`_
    between `col1` and `col2`.


    Parameters
    ----------
    col1, col2: FExpr
        Input columns.

    return: Expr
        f-expression having one row, one column and the covariance between
        `col1` and `col2` as the value. If one of the input columns is non-numeric,
        the value is `NA`. The output column stype is `float32` if both `col1`
        and `col2` are `float32`, and `float64` in all the other cases.


    Examples
    --------

    .. code-block:: python

        >>> from datatable import dt, f
        >>>
        >>> DT = dt.Frame(A = [0, 1, 2, 3], B = [0, 2, 4, 6])
        >>> DT
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     0      0
         1 |     1      2
         2 |     2      4
         3 |     3      6
        [4 rows x 2 columns]

        >>> DT[:, dt.cov(f.A, f.B)]
           |      C0
           | float64
        -- + -------
         0 | 3.33333
        [1 row x 1 column]


    See Also
    --------
    - :func:`corr()` -- function to calculate correlation between two columns.
