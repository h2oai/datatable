
.. xfunction:: datatable.sd
    :src: src/core/expr/head_reduce_unary.cc sd_reducer
    :cvar: doc_dt_sd
    :signature: sd(cols)

    Calculate the standard deviation for each column from `cols`.


    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: Expr
        f-expression having one row, and the same names and number of columns
        as in `cols`. The column stypes are `float32` for
        `float32` columns, and `float64` for all the other numeric types.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-numeric type.


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


    Get the standard deviation of column A::

        >>> DT[:, dt.sd(f.A)]
           |       A
           | float64
        -- + -------
         0 | 1.29099
        [1 row x 1 column]


    Get the standard deviation of columns A and B::

        >>> DT[:, dt.sd([f.A, f.B])]
           |       A        B
           | float64  float64
        -- + -------  -------
         0 | 1.29099  2.58199
        [1 row x 2 columns]


    See Also
    --------

    - :func:`mean()` -- function to calculate mean values.
    - :func:`median()` -- function to calculate median values.
