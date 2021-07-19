
.. xfunction:: datatable.time.ymdt
    :src: src/core/expr/time/fexpr_ymdt.cc pyfn_ymdt
    :tests: tests/time/test-ymdt.py
    :cvar: doc_time_ymdt
    :signature: ymdt(year, month, day, hour, minute, second, nanosecond=0, *, date=None)

    .. x-version-added:: 1.0.0

    Create a time64 column out of `year`, `month`, `day`, `hour`, `minute`,
    `second` and `nanosecond` (optional) components. Alternatively, instead
    of year-month-day triple you can pass `date` argument of type `date32`.

    This function performs range checks on `month` and `day` columns: if a
    certain combination of year/month/day is not valid in the Gregorian
    calendar, then an NA value will be produced in that row.

    At the same time, there are no range checks for time components. Thus,
    you can, for example, pass `second=3600` instead of `hour=1`.


    Parameters
    ----------
    year: FExpr[int]
        The year part of the resulting time64 column.

    month: FExpr[int]
        The month part of the resulting time64 column. Values in this column
        must be in the 1 .. 12 range.

    day: FExpr[int]
        The day part of the resulting time64 column. Values in this column
        should be from 1 to ``last_day_of_month(year, month)``.

    hour: FExpr[int]
        The hour part of the resulting time64 column.

    minute: FExpr[int]
        The minute part of the resulting time64 column.

    second: FExpr[int]
        The second part of the resulting time64 column.

    nanosecond: FExpr[int]
        The nanosecond part of the resulting time64 column. This parameter
        is optional.

    date: FExpr[date32]
        The date component of the resulting time64 column. This parameter,
        if given, replaces parameters `year`, `month` and `day`, and cannot
        be used together with them.

    return: FExpr[time64]


    Examples
    --------
    >>> DT = dt.Frame(Y=[2001, 2003, 2005, 2020, 1960],
    ...               M=[1, 5, 4, 11, 8],
    ...               D=[12, 18, 30, 1, 14],
    ...               h=[7, 14, 22, 23, 12],
    ...               m=[15, 30, 0, 59, 0],
    ...               s=[12, 23, 0, 59, 27],
    ...               ns=[0, 0, 0, 999999000, 123000])
    >>> DT[:, [f[:], dt.time.ymdt(f.Y, f.M, f.D, f.h, f.m, f.s, f.ns)]]
       |     Y      M      D      h      m      s         ns  C0
       | int32  int32  int32  int32  int32  int32      int32  time64
    -- + -----  -----  -----  -----  -----  -----  ---------  --------------------------
     0 |  2001      1     12      7     15     12          0  2001-01-12T07:15:12
     1 |  2003      5     18     14     30     23          0  2003-05-18T14:30:23
     2 |  2005      4     30     22      0      0          0  2005-04-30T22:00:00
     3 |  2020     11      1     23     59     59  999999000  2020-11-01T23:59:59.999999
     4 |  1960      8     14     12      0     27     123000  1960-08-14T12:00:27.000123
    [5 rows x 8 columns]
