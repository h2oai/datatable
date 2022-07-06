
.. xfunction:: datatable.bfill
    :src: src/core/expr/fexpr_bfill.cc pyfn_bfill
    :tests: tests/dt/test-bfill.py
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
        >>> DT = dt.Frame([(datetime(2020,1,1),'apple',1,),
                           (datetime(2020,1,2),'apple',2),
                           (datetime(2020,1,3),'apple',None),
                           (datetime(2020,1,4),'apple',None),
                           (datetime(2020,1,5),'apple',5),
                           (datetime(2020,1,1),'orange',4,),
                           (datetime(2020,1,2),'orange',50),
                           (datetime(2020,1,3),'orange',None),
                           (datetime(2020,1,4),'orange',55),
                           (datetime(2020,1,5),'orange',None),
                           (datetime(2020,1,1),'banana',None),
                           (datetime(2020,1,2),'banana',None),
                           (datetime(2020,1,3),'banana',None),
                           (datetime(2020,1,4),'banana',None),
                           (datetime(2020,1,5),'banana',None)],
                           names = ['date','fruit','quantity']
                         )
           | date                 fruit   quantity
           | time64               str32      int32
        -- + -------------------  ------  --------
         0 | 2020-01-01T00:00:00  apple          1
         1 | 2020-01-02T00:00:00  apple          2
         2 | 2020-01-03T00:00:00  apple         NA
         3 | 2020-01-04T00:00:00  apple         NA
         4 | 2020-01-05T00:00:00  apple          5
         5 | 2020-01-01T00:00:00  orange         4
         6 | 2020-01-02T00:00:00  orange        50
         7 | 2020-01-03T00:00:00  orange        NA
         8 | 2020-01-04T00:00:00  orange        55
         9 | 2020-01-05T00:00:00  orange        NA
        10 | 2020-01-01T00:00:00  banana        NA
        11 | 2020-01-02T00:00:00  banana        NA
        12 | 2020-01-03T00:00:00  banana        NA
        13 | 2020-01-04T00:00:00  banana        NA
        14 | 2020-01-05T00:00:00  banana        NA
       [15 rows x 3 columns]


    Forward fill on a single column::

        >>> DT[:, dt.ffill(f.quantity)]
           |     A
           | int32
        -- + -----
         0 |     2
         1 |     2
         2 |     5
         3 |     5
         4 |     5
        [5 rows x 1 column]

    Forward fill in the presence of :func:`by()`::

        >>> DT[:, dt.ffill(f.quantity), by('fruit')]
           | D          A     B        C
           | str32  int32  void  float64
        -- + -----  -----  ----  -------
         0 | a          2    NA    5.4  
         1 | a          2    NA    5.4  
         2 | b          5    NA    2.2  
         3 | b          5    NA    4.323
         4 | b          5    NA    4.323
        [5 rows x 4 columns]
