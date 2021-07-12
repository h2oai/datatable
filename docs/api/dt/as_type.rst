
.. xfunction:: datatable.as_type
    :src: src/core/expr/fexpr_astype.cc pyfn_astype
    :tests: tests/dt/test-astype.py
    :cvar: doc_dt_as_type
    :signature: as_type(cols, new_type)

    .. x-version-added:: 1.0

    Convert columns `cols` into the prescribed stype.

    This function does not modify the data in the original column. Instead
    it returns a new column which converts the values into the new type on
    the fly.

    Parameters
    ----------
    cols: FExpr
        Single or multiple columns that need to be converted.

    new_type: Type | stype
        Target type.

    return: FExpr
        The output will have the same number of rows and columns as the
        input; column names will be preserved too.


    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f, as_type
        >>>
        >>> df = dt.Frame({'A': ['1', '1', '2', '1', '2'],
        ...                'B': [None, '2', '3', '4', '5'],
        ...                'C': [1, 2, 1, 1, 2]})
        >>> df
           | A      B          C
           | str32  str32  int32
        -- + -----  -----  -----
         0 | 1      NA         1
         1 | 1      2          2
         2 | 2      3          1
         3 | 1      4          1
         4 | 2      5          2
        [5 rows x 3 columns]


    Convert column A from string to integer type::

        >>> df[:, as_type(f.A, int)]
           |     A
           | int64
        -- + -----
         0 |     1
         1 |     1
         2 |     2
         3 |     1
         4 |     2
        [5 rows x 1 column]


    The exact dtype can be specified::

        >>> df[:, as_type(f.A, dt.Type.int32)]
           |     A
           | int32
        -- + -----
         0 |     1
         1 |     1
         2 |     2
         3 |     1
         4 |     2
        [5 rows x 1 column]


    Convert multiple columns to different types::

        >>> df[:, [as_type(f.A, int), as_type(f.C, dt.str32)]]
           |     A  C
           | int64  str32
        -- + -----  -----
         0 |     1  1
         1 |     1  2
         2 |     2  1
         3 |     1  1
         4 |     2  2
        [5 rows x 2 columns]
