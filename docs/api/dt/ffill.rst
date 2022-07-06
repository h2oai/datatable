
.. xfunction:: datatable.ffill
    :src: src/core/expr/fexpr_ffill_bfill.cc pyfn_ffill
    :tests: test-bfill_ffill.py
    :cvar: doc_dt_ffill
    :signature: ffill(cols)

    .. x-version-added:: 1.1.0

    For each column from `cols` fill the null values forward with the last non-null value. 
    In the presence of :func:`by()`, the last non-null value is propagated forward within each group.

    Parameters
    ----------
    cols: FExpr
        Input data for filling the nulls values forward.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with the previous non-null values.

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


    Forward fill on a single column::

        >>> DT[:, dt.ffill(f.var1)]
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

    Forward fill on multiple columns::

        >>> DT[:, dt.ffill(f['var1':])]
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



    Forward fill in the presence of :func:`by()`::

        >>> DT[:, dt.ffill(f['var1':]), by('building')]
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

