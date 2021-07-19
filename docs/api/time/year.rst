
.. xfunction:: datatable.time.year
    :src: src/core/expr/time/fexpr_year_month_day.cc pyfn_year_month_day
    :tests: tests/time/test-year-month-day.py
    :cvar: doc_time_year
    :signature: year(date)

    .. x-version-added:: 1.0.0

    Retrieve the "year" component of a date32 or time64 column.


    Parameters
    ----------
    date: FExpr[date32] | FExpr[time64]
        A column for which you want to compute the year part.

    return: FExpr[int32]
        The year part of the source column.


    Examples
    --------
    >>> DT = dt.Frame([1, 1000, 100000], stype='date32')
    >>> DT[:, {'date': f[0], 'year': dt.time.year(f[0])}]
       | date         year
       | date32      int32
    -- + ----------  -----
     0 | 1970-01-02   1970
     1 | 1972-09-27   1972
     2 | 2243-10-17   2243
    [3 rows x 2 columns]


    See Also
    --------
    - :func:`month()` -- retrieve the "month" component of a date
    - :func:`day()` -- retrieve the "day" component of a date
