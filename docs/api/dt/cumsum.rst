
.. xfunction:: datatable.cumsum
    :src: src/core/expr/fexpr_cumsumprod.cc pyfn_cumsum
    :tests: tests/dt/test-cumsum.py
    :cvar: doc_dt_cumsum
    :signature: cumsum(cols)

    .. x-version-added:: 1.1.0

    For each column from `cols` calculate cumulative sum. The sum of
    the missing values is calculated as zero. In the presence of :func:`by()`,
    the cumulative sum is computed within each group.

    Parameters
    ----------
    cols: FExpr
        Input data for cumulative sum calculation.

    reverse: bool
        If ``False``, the cumulative sum is computed from the last row
        to the first row. if ``True``, the cumulative sum is computed 
        from the first row to the last row.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with the respective cumulative sums.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-numeric type.


    Examples
    --------

    Create a sample datatable frame::

        >>> from datatable import dt, f, by
        >>> DT = dt.Frame({"A": [2, None, 5, -1, 0],
        ...                "B": [None, None, None, None, None],
        ...                "C": [5.4, 3, 2.2, 4.323, 3], 
        ...                "D": ['a', 'a', 'b', 'b', 'b']})
           |     A     B        C  D    
           | int32  void  float64  str32
        -- + -----  ----  -------  -----
         0 |     2    NA    5.4    a    
         1 |    NA    NA    3      a    
         2 |     5    NA    2.2    b    
         3 |    -1    NA    4.323  b    
         4 |     0    NA    3      b    
        [5 rows x 4 columns]


    Calculate cumulative sum in a single column::

        >>> DT[:, dt.cumsum(f.A)]
           |     A
           | int64
        -- + -----
         0 |     2
         1 |     2
         2 |     7
         3 |     6
         4 |     6
        [5 rows x 1 column]


    Calculate the cumulative sum when `reverse` is `True`::

        >>> DT[:, dt.cumsum(f.A, reverse=True)]
           |     A
           | int64
        -- + -----
         0 |     6
         1 |     4
         2 |     4
         3 |    -1
         4 |     0
        [5 rows x 1 column]
        

    Calculate cumulative sums in multiple columns::

        >>> DT[:, dt.cumsum(f[:-1])]
           |     A      B        C
           | int64  int64  float64
        -- + -----  -----  -------
         0 |     2      0    5.4  
         1 |     2      0    8.4  
         2 |     7      0   10.6  
         3 |     6      0   14.923
         4 |     6      0   17.923
        [5 rows x 3 columns]


    In the presence of :func:`by()` calculate cumulative sums within each group::

        >>> DT[:, dt.cumsum(f[:]), by('D')]
           | D          A      B        C
           | str32  int64  int64  float64
        -- + -----  -----  -----  -------
         0 | a          2      0    5.4  
         1 | a          2      0    8.4  
         2 | b          5      0    2.2  
         3 | b          4      0    6.523
         4 | b          4      0    9.523
        [5 rows x 4 columns]
