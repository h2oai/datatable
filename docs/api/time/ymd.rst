
.. xfunction:: datatable.time.ymd
    :src: src/core/expr/time/fexpr_ymd.cc pyfn_ymd
    :tests: tests/time/test-ymd.py
    :cvar: doc_time_ymd
    :signature: ymd(year, month, day)

    .. x-version-added:: 1.0.0

    Create a date32 column out of `year`, `month` and `day` components.

    This function performs range checks on `month` and `day` columns: if a
    certain combination of year/month/day is not valid in the Gregorian
    calendar, then an NA value will be produced in that row.


    Parameters
    ----------
    year: FExpr[int]
        The year part of the resulting date32 column.

    month: FExpr[int]
        The month part of the resulting date32 column. Values in this column
        are expected to be in the 1 .. 12 range.

    day: FExpr[int]
        The day part of the resulting date32 column. Values in this column
        should be from 1 to ``last_day_of_month(year, month)``.

    return: FExpr[date32]


    Examples
    --------
    >>> DT = dt.Frame(y=[2005, 2010, 2015], m=[2, 3, 7])
    >>> DT[:, dt.time.ymd(f.y, f.m, 30)]
       | C0
       | date32
    -- + ----------
     0 | NA
     1 | 2010-03-30
     2 | 2015-07-30
    [3 rows x 1 column]
