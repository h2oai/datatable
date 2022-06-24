
.. xfunction:: datatable.cumprod
    :src: src/core/expr/fexpr_cumsumprod.cc pyfn_cumprod
    :tests: tests/dt/test-cumprod.py
    :cvar: doc_dt_cumprod
    :signature: cumprod(cols)

    .. x-version-added:: 1.1.0

    For each column from `cols` calculate cumulative product. The product of
    the missing values is calculated as one. In the presence of :func:`by()`,
    the cumulative product is computed within each group.

    Parameters
    ----------
    cols: FExpr
        Input data for cumulative product calculation.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with the respective cumulative products.

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


    Calculate cumulative product in a single column::

        >>> DT[:, dt.cumprod(f.A)]
           |     A
           | int64
        -- + -----
         0 |     2
         1 |     2
         2 |    10
         3 |   -10
         4 |     0
        [5 rows x 1 column]


    Calculate cumulative products in multiple columns::

        >>> DT[:, dt.cumprod(f[:-1])]
           |     A      B        C
           | int64  int64  float64
        -- + -----  -----  -------
         0 |     2      1    5.4  
         1 |     2      1   16.2  
         2 |    10      1   35.64 
         3 |   -10      1  154.072
         4 |     0      1  462.215
        [5 rows x 3 columns]


    In the presence of :func:`by()` calculate cumulative products within each group::

        >>> DT[:, dt.cumprod(f[:]), by('D')]
           | D          A      B        C
           | str32  int64  int64  float64
        -- + -----  -----  -----  -------
         0 | a          2      1   5.4   
         1 | a          2      1  16.2   
         2 | b          5      1   2.2   
         3 | b         -5      1   9.5106
         4 | b          0      1  28.5318
        [5 rows x 4 columns]

