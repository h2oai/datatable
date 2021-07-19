
.. xfunction:: datatable.time.month
    :src: src/core/expr/time/fexpr_year_month_day.cc pyfn_year_month_day
    :tests: tests/time/test-year-month-day.py
    :cvar: doc_time_month
    :signature: month(date)

    .. x-version-added:: 1.0.0

    Retrieve the "month" component of a date32 or time64 column.


    Parameters
    ----------
    date: FExpr[date32] | FExpr[time64]
        A column for which you want to compute the month part.

    return: FExpr[int32]
        The month part of the source column.


    Examples
    --------
    >>> DT = dt.Frame([1, 1000, 100000], stype='date32')
    >>> DT[:, {'date': f[0], 'month': dt.time.month(f[0])}]
       | date        month
       | date32      int32
    -- + ----------  -----
     0 | 1970-01-02      1
     1 | 1972-09-27      9
     2 | 2243-10-17     10
    [3 rows x 2 columns]


    See Also
    --------
    - :func:`year()` -- retrieve the "year" component of a date
    - :func:`day()` -- retrieve the "day" component of a date
