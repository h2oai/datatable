
.. xfunction:: datatable.cummin
    :src: src/core/expr/fexpr_cumminmax.cc pyfn_cummin
    :tests: tests/dt/test-cumminmax.py
    :cvar: doc_dt_cummin
    :signature: cummin(cols)

    .. x-version-added:: 1.1.0

    For each column from `cols` calculate cumulative minimum. In the presence of :func:`by()`,
    the cumulative minimum is computed within each group.

    Parameters
    ----------
    cols: FExpr
        Input data for cumulative minimum calculation.

    reverse: bool
        If ``False``, the cumulative minimum is computed from the last row
        to the first row. if ``True``, the cumulative minimum is computed 
        from the first row to the last row.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with the respective cumulative minimums.

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


    Calculate the cumulative minimum in a single column::

        >>> DT[:, dt.cummin(f.A)]
           |     A
           | int32
        -- + -----
         0 |     2
         1 |     2
         2 |     2
         3 |    -1
         4 |    -1
        [5 rows x 1 column]
        

    Calculate the cumulative minimum when `reverse` is `True`::

        >>> DT[:, dt.cummin(f.A, reverse=True)]
           |     A
           | int32
        -- + -----
         0 |    -1
         1 |    -1
         2 |    -1
         3 |    -1
         4 |     0
        [5 rows x 1 column]


    Calculate the cumulative minimum in multiple columns::

        >>> DT[:, dt.cummin(f[:-1])]
           |     A     B        C
           | int32  void  float64
        -- + -----  ----  -------
         0 |     2    NA      5.4
         1 |     2    NA      3  
         2 |     2    NA      2.2
         3 |    -1    NA      2.2
         4 |    -1    NA      2.2
        [5 rows x 3 columns]


    In the presence of :func:`by()` calculate the cumulative minimum within each group::

        >>> DT[:, dt.cummin(f[:]), by('D')]
           | D          A     B        C
           | str32  int32  void  float64
        -- + -----  -----  ----  -------
         0 | a          2    NA      5.4
         1 | a          2    NA      3  
         2 | b          5    NA      2.2
         3 | b         -1    NA      2.2
         4 | b         -1    NA      2.2
        [5 rows x 4 columns]
