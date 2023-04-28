
.. xfunction:: datatable.math.isna
    :src: src/core/expr/fexpr_isna.cc pyfn_isna
    :tests: tests/math/test-isna.py
    :cvar: doc_dt_isna
    :signature: isna(cols)

    Tests if the column elements are missing values. 

    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: FExpr
        f-expression that returns `0` for valid elements and `1` otherwise. 
        All the resulting columns will have `bool8` stypes 
        and as many rows/columns as there are in `cols`.

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


