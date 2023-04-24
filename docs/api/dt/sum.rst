
.. xfunction:: datatable.sum
    :src: src/core/expr/fexpr_sumprod.cc pyfn_sum
    :tests: tests/test-reduce.py
    :cvar: doc_dt_sum
    :signature: sum(cols)

    Calculate the sum of values for each column from `cols`. The sum of
    the missing values is calculated as zero.


    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: FExpr
        f-expression having one row, and the same names and number of columns
        as in `cols`. The column types are :attr:`int64 <dt.Type.int64>` for
        void, boolean and integer columns, :attr:`float32 <dt.Type.float32>`
        for :attr:`float32 <dt.Type.float32>` columns and
        :attr:`float64 <dt.Type.float64>` for :attr:`float64 <dt.Type.float64>`
        columns.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-numeric type.


    Examples
    --------

    .. code-block:: python

        >>> from datatable import dt, f, by
        >>>
        >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
        ...                'B': [None, 2, 3, 4, 5],
        ...                'C': [1, 2, 1, 1, 2]})
        >>> df
           |     A      B      C
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     1     NA      1
         1 |     1      2      2
         2 |     2      3      1
         3 |     1      4      1
         4 |     2      5      2
        [5 rows x 3 columns]

    Get the sum of column A::

        >>> df[:, dt.sum(f.A)]
           |     A
           | int64
        -- + -----
         0 |     7
        [1 row x 1 column]

    Get the sum of multiple columns::

        >>> df[:, [dt.sum(f.A), dt.sum(f.B)]]
           |     A      B
           | int64  int64
        -- + -----  -----
         0 |     7     14
        [1 row x 2 columns]

    Same as above, but more convenient::

        >>> df[:, dt.sum(f[:2])]
           |     A      B
           | int64  int64
        -- + -----  -----
         0 |     7     14
        [1 row x 2 columns]

    In the presence of :func:`by()`, it returns the sum of the specified columns per group::

        >>> df[:, [dt.sum(f.A), dt.sum(f.B)], by(f.C)]
           |     C      A      B
           | int32  int64  int64
        -- + -----  -----  -----
         0 |     1      4      7
         1 |     2      3      7
        [2 rows x 3 columns]


    See Also
    --------
    - :meth:`.sum() <dt.Frame.sum>` -- the corresponding Frame's method
      for multi-column frames.

    - :meth:`.sum1() <dt.Frame.sum1>` -- the corresponding Frame's method
      for single-column frames that returns a scalar value.

    - :func:`count()` -- function to calculate a number of non-missing values.
