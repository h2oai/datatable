
.. xfunction:: datatable.fillna
    :src: src/core/expr/fexpr_fillna.cc pyfn_fillna
    :tests: tests/dt/test-fillna.py
    :cvar: doc_dt_fillna
    :signature: fillna(cols, value=None, reverse=False)

    .. x-version-added:: 1.1.0

    For each column from `cols` either fill the missing values with a single value, 
    or fill with the previous or subsequent non-missing values. 
    In the presence of :func:`by()` the filling is performed group-wise.

    Parameters
    ----------
    cols: FExpr
        Input columns.

    value: Scalar or FExpr to replace the missing values.

    reverse: bool
        If ``False``, the missing values are filled by using the closest
        previous non-missing values as a replacement. if ``True``,
        the closest subsequent non-missing values are used.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with a single value, or the previous/subsequent non-missing values.


    Examples
    --------

    Create a sample datatable frame::

        >>> from datatable import dt, f, by
        >>> DT = dt.Frame({'building': ['a', 'a', 'b', 'b', 'a', 'a', 'b', 'b'],
        ...                'var1': [1.5, None, 2.1, 2.2, 1.2, 1.3, 2.4, None],
        ...                'var2': [100, 110, 105, None, 102, None, 103, 107],
        ...                'var3': [10, 11, None, None, None, None, None, None],
        ...                'var4': [1, 2, 3, 4, 5, 6, 7, 8]})
           | building     var1   var2   var3   var4
           | str32     float64  int32  int32  int32
        -- + --------  -------  -----  -----  -----
         0 | a             1.5    100     10      1
         1 | a            NA      110     11      2
         2 | b             2.1    105     NA      3
         3 | b             2.2     NA     NA      4
         4 | a             1.2    102     NA      5
         5 | a             1.3     NA     NA      6
         6 | b             2.4    103     NA      7
         7 | b            NA      107     NA      8
        [8 rows x 5 columns]

    Fill a single column with a specified value::

       >>> DT[:, dt.fillna(f.var1, 2)]
          |    var1
          | float64
       -- + -------
        0 |     1.5
        1 |     2  
        2 |     2.1
        3 |     2.2
        4 |     1.2
        5 |     1.3
        6 |     2.4
        7 |     2  
       [8 rows x 1 column]

    Fill multiple columns with a single value::

        >>> DT[:, dt.fillna(f[1:], 2)]
           |    var1   var2   var3   var4
           | float64  int32  int32  int32
        -- + -------  -----  -----  -----
         0 |     1.5    100     10      1
         1 |     2      110     11      2
         2 |     2.1    105      2      3
         3 |     2.2      2      2      4
         4 |     1.2    102      2      5
         5 |     1.3      2      2      6
         6 |     2.4    103      2      7
         7 |     2      107      2      8
        [8 rows x 4 columns]

    Fill each column with a different value, in the presence of :func:`by()`::

        >>> DT[:, dt.fillna(f[:], dt.mean(f[:])), by('building')]
           | building     var1     var2     var3     var4
           | str32     float64  float64  float64  float64
        -- + --------  -------  -------  -------  -------
         0 | a         1.5          100     10          1
         1 | a         1.33333      110     11          2
         2 | a         1.2          102     10.5        5
         3 | a         1.3          104     10.5        6
         4 | b         2.1          105     NA          3
         5 | b         2.2          105     NA          4
         6 | b         2.4          103     NA          7
         7 | b         2.23333      107     NA          8
        [8 rows x 5 columns]
    
    Fill down on a single column::
        
        >>> DT[:, dt.fillna(f.var1)]
           |    var1
           | float64
        -- + -------
         0 |     1.5
         1 |     1.5
         2 |     2.1
         3 |     2.2
         4 |     1.2
         5 |     1.3
         6 |     2.4
         7 |     2.4
        [8 rows x 1 column]
         

    Fill up on a single column::

        >>> DT[:, dt.fillna(f.var1, reverse = True)]
           |    var1
           | float64
        -- + -------
         0 |     1.5
         1 |     2.1
         2 |     2.1
         3 |     2.2
         4 |     1.2
         5 |     1.3
         6 |     2.4
         7 |    NA
        [8 rows x 1 column]


    Fill down on multiple columns::

         >>> DT[:, dt.fillna(f['var1':])]
            |    var1   var2   var3   var4
            | float64  int32  int32  int32
         -- + -------  -----  -----  -----
          0 |     1.5    100     10      1
          1 |     1.5    110     11      2
          2 |     2.1    105     11      3
          3 |     2.2    105     11      4
          4 |     1.2    102     11      5
          5 |     1.3    102     11      6
          6 |     2.4    103     11      7
          7 |     2.4    107     11      8
         [8 rows x 4 columns]


    Fill up on multiple columns::

        >>> DT[:, dt.fillna(f['var1':], reverse = True)]
           |    var1   var2   var3   var4
           | float64  int32  int32  int32
        -- + -------  -----  -----  -----
         0 |     1.5    100     10      1
         1 |     2.1    110     11      2
         2 |     2.1    105     NA      3
         3 |     2.2    102     NA      4
         4 |     1.2    102     NA      5
         5 |     1.3    103     NA      6
         6 |     2.4    103     NA      7
         7 |    NA      107     NA      8
        [8 rows x 4 columns]


    Fill down in the presence of :func:`by()`::

        >>> DT[:, dt.fillna(f['var1':]), by('building')]
           | building     var1   var2   var3   var4
           | str32     float64  int32  int32  int32
        -- + --------  -------  -----  -----  -----
         0 | a             1.5    100     10      1
         1 | a             1.5    110     11      2
         2 | a             1.2    102     11      5
         3 | a             1.3    102     11      6
         4 | b             2.1    105     NA      3
         5 | b             2.2    105     NA      4
         6 | b             2.4    103     NA      7
         7 | b             2.4    107     NA      8
        [8 rows x 5 columns]


    Fill up in the presence of :func:`by()`::

        >>> DT[:, dt.fillna(f['var1':], reverse = True), by('building')]
           | building     var1   var2   var3   var4
           | str32     float64  int32  int32  int32
        -- + --------  -------  -----  -----  -----
         0 | a             1.5    100     10      1
         1 | a             1.2    110     11      2
         2 | a             1.2    102     NA      5
         3 | a             1.3     NA     NA      6
         4 | b             2.1    105     NA      3
         5 | b             2.2    103     NA      4
         6 | b             2.4    103     NA      7
         7 | b            NA      107     NA      8
        [8 rows x 5 columns]
