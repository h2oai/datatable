
.. xfunction:: datatable.corr
    :src: src/core/expr/head_reduce_binary.cc compute_corr
    :tests: tests/test-reduce.py
    :cvar: doc_dt_corr
    :signature: corr(col1, col2)

    Calculate the
    `Pearson correlation <https://en.wikipedia.org/wiki/Pearson_correlation_coefficient>`_
    between `col1` and `col2`.

    Parameters
    ----------
    col1, col2: Expr
        Input columns.

    return: Expr
        f-expression having one row, one column and the correlation coefficient
        as the value. If one of the columns is non-numeric, the value is `NA`.
        The column stype is `float32` if both `col1` and `col2` are `float32`,
        and `float64` in all the other cases.


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

        >>> DT[:, dt.corr(f.A, f.B)]
           |      C0
           | float64
        -- + -------
         0 |       1
        [1 row x 1 column]


    See Also
    --------
    - :func:`cov()` -- function to calculate covariance between two columns.
