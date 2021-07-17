
.. xfunction:: datatable.time.day_of_week
    :src: src/core/expr/time/fexpr_day_of_week.cc pyfn_day_of_week
    :tests: tests/time/test-day-of-week.py
    :cvar: doc_time_day_of_week
    :signature: day_of_week(date)

    .. x-version-added:: 1.0.0

    For a given date column compute the corresponding days of week.

    Days of week are returned as integers from 1 to 7, where 1 represents
    Monday, and 7 is Sunday. Thus, the return value of this function
    matches the ISO standard.


    Parameters
    ----------
    date: FExpr[date32] | FExpr[time64]
        The date32 (or time64) column for which you need to calculate days of week.

    return: FExpr[int32]
        An integer column, with values between 1 and 7 inclusive.


    Examples
    --------
    >>> DT = dt.Frame([18000, 18600, 18700, 18800, None], stype='date32')
    >>> DT[:, {"date": f[0], "day-of-week": dt.time.day_of_week(f[0])}]
       | date        day-of-week
       | date32            int32
    -- + ----------  -----------
     0 | 2019-04-14            7
     1 | 2020-12-04            5
     2 | 2021-03-14            7
     3 | 2021-06-22            2
     4 | NA                   NA
    [5 rows x 2 columns]
