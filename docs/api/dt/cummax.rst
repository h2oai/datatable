
.. xfunction:: datatable.cummax
    :src: src/core/expr/fexpr_cummax.cc pyfn_cummax
    :tests: tests/dt/test-cummax.py
    :cvar: doc_dt_cummax
    :signature: cummax(cols)

    .. x-version-added:: 1.1.0

    For each column from `cols` calculate cumulative maximum. The maximum of
    the missing values is calculated as zero. In the presence of :func:`by()`,
    the cumulative maximum is computed per group.

    Parameters
    ----------
    cols: FExpr
        Input data for cumulative maximum calculation.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with the respective cumulative maximum.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-numeric type.


    Examples
    --------

    Create a sample datatable frame::

        >>> from datatable import dt, f
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


    Calculate the cumulative maximum in a single column::

        >>> DT[:, dt.cummax(f.A)]
           |     A
           | int64
        -- + -----
         0 |     2
         1 |     2
         2 |     5
         3 |     5
         4 |     5
        [5 rows x 1 column]


    Calculate the cumulative maximum in multiple columns::

        >>> DT[:, dt.cummax(f[:-1])]
           |     A     B        C
           | int64  void  float64
        -- + -----  ----  -------
         0 |     2    NA      5.4
         1 |     2    NA      5.4
         2 |     5    NA      5.4
         3 |     5    NA      5.4
         4 |     5    NA      5.4
        [5 rows x 3 columns]


    Calculate the cumulative maximum per group in the presence of :func:`by()`::

        >>> DT[:, dt.cummax(f[:]), by('D')]
           | D          A     B        C
           | str32  int64  void  float64
        -- + -----  -----  ----  -------
         0 | a          2    NA    5.4  
         1 | a          2    NA    5.4  
         2 | b          5    NA    2.2  
         3 | b          5    NA    4.323
         4 | b          5    NA    4.323
        [5 rows x 4 columns]
