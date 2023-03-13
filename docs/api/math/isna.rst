
.. xfunction:: datatable.math.isna
    :src: src/core/expr/fexpr_isna.cc pyfn_isna
    :tests: tests/math/test-isna.py
    :cvar: doc_math_isna
    :signature: isna(x)

    Returns `True` if the argument is NA, and `False` otherwise.

    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: Expr
        f-expression having one row, and the same names and number of columns
        as in `cols`. All the returned column stypes are `int8`.

    Examples
    --------

    .. code-block:: python

        >>> from datatable import dt, f
        >>> from datetime import datetime
        >>> DT = dt.Frame({'age': [5.0, 6.0, None],
        ...                'born': [None,
        ...                datetime(1939, 5, 27, 0, 0),
        ...                datetime(1940, 4, 25, 0, 0)],
        ...                'name': ['Alfred', 'Batman', ''],
        ...                'toy': [None, 'Batmobile', 'Joker']})
        >>> DT
           |     age  born                 name    toy      
           | float64  time64               str32   str32    
        -- + -------  -------------------  ------  ---------
         0 |       5  NA                   Alfred  NA       
         1 |       6  1939-05-27T00:00:00  Batman  Batmobile
         2 |      NA  1940-04-25T00:00:00          Joker    
        [3 rows x 4 columns]
        >>> DT[:, dt.math.isna(f[:])]
           |   age   born   name    toy
           | bool8  bool8  bool8  bool8
        -- + -----  -----  -----  -----
         0 |     0      1      0      1
         1 |     0      0      0      0
         2 |     1      0      0      0
        [3 rows x 4 columns]

