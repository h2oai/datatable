
.. xfunction:: datatable.time.minute
    :src: src/core/expr/time/fexpr_hour_min_sec.cc pyfn_hour_min_sec
    :tests: tests/time/test-hour-min-sec.py
    :cvar: doc_time_minute
    :signature: minute(time)

    .. x-version-added:: 1.0.0

    Retrieve the "minute" component of a time64 column. The produced column
    will have values in the range [0; 59].


    Parameters
    ----------
    time: FExpr[time64]
        A column for which you want to compute the minute part.

    return: FExpr[int32]
        The minute part of the source column.


    Examples
    --------
    >>> from datetime import datetime as d
    >>> DT = dt.Frame([d(2020, 5, 11, 12, 0, 0), d(2021, 6, 14, 16, 10, 59, 394873)])
    >>> DT[:, {'time': f[0], 'minute': dt.time.minute(f[0])}]
       | time                        minute
       | time64                       int32
    -- + --------------------------  ------
     0 | 2020-05-11T12:00:00              0
     1 | 2021-06-14T16:10:59.394873      10
    [2 rows x 2 columns]


    See Also
    --------
    - :func:`hour()` -- retrieve the "hour" component of a timestamp
    - :func:`second()` -- retrieve the "second" component of a timestamp
    - :func:`nanosecond()` -- retrieve the "nanosecond" component of a timestamp
