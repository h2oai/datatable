
.. xfunction:: datatable.bfill
    :src: src/core/expr/fexpr_ffill_bfill.cc pyfn_bfill
    :tests: test-bfill_ffill.py
    :cvar: doc_dt_bfill
    :signature: bfill(cols)

    .. x-version-added:: 1.1.0

    For each column from `cols` fill the null values backward with the last non-null value. 
    In the presence of :func:`by()`, the last non-null value is propagated backward within each group.

    Parameters
    ----------
    cols: FExpr
        Input data for filling the nulls values backward.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with the next non-null values.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-numeric type.


    Examples
    --------

    Create a sample datatable frame::

        >>> from datatable import dt, f, by
        >>> from datetime import datetime
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


    Backward fill on a single column::

        >>> DT[:, dt.bfill(f.var1)]
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

    Backward fill on multiple columns::

        >>> DT[:, dt.bfill(f['var1':])]
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



    Backward fill in the presence of :func:`by()`::

        >>> DT[:, dt.bfill(f['var1':]), by('building')]
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

