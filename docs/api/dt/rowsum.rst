
.. xfunction:: datatable.rowsum
    :src: src/core/expr/fnary/rowsum.cc FExpr_RowSum::apply_function
    :tests: tests/ijby/test-rowwise.py
    :cvar: doc_dt_rowsum
    :signature: rowsum(*cols)

    For each row, calculate the sum of all values in `cols`. Missing values
    are treated as if they are zeros and skipped during the calcultion.


    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: FExpr
        f-expression consisting of one column and the same number
        of rows as in `cols`. The stype of the resulting column
        will be the smallest common stype calculated for `cols`,
        but not less than `int32`.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-numeric type.

    Examples
    --------
    ::

        >>> from datatable import dt, f, rowsum
        >>> DT = dt.Frame({'a': [1,2,3],
        ...                'b': [2,3,4],
        ...                'c':['dd','ee','ff'],
        ...                'd':[5,9,1]})
        >>> DT
           |     a      b  c          d
           | int32  int32  str32  int32
        -- + -----  -----  -----  -----
         0 |     1      2  dd         5
         1 |     2      3  ee         9
         2 |     3      4  ff         1
        [3 rows x 4 columns]


    ::

        >>> DT[:, rowsum(f[int])]
           |    C0
           | int32
        -- + -----
         0 |     8
         1 |    14
         2 |     8
        [3 rows x 1 column]

    ::

        >>> DT[:, rowsum(f.a, f.b)]
           |    C0
           | int32
        -- + -----
         0 |     3
         1 |     5
         2 |     7
        [3 rows x 1 column]

    The above code could also be written as ::

        >>> DT[:, f.a + f.b]
           |    C0
           | int32
        -- + -----
         0 |     3
         1 |     5
         2 |     7
        [3 rows x 1 column]



    See Also
    --------
    - :func:`rowcount()` -- count non-missing values row-wise.
