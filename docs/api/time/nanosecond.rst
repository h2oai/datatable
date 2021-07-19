
.. xfunction:: datatable.time.nanosecond
    :src: src/core/expr/time/fexpr_hour_min_sec.cc pyfn_hour_min_sec
    :tests: tests/time/test-hour-min-sec.py
    :cvar: doc_time_nanosecond
    :signature: nanosecond(time)

    .. x-version-added:: 1.0.0

    Retrieve the "nanosecond" component of a time64 column. The produced
    column will have values in the range [0; 999999999].


    Parameters
    ----------
    time: FExpr[time64]
        A column for which you want to compute the nanosecond part.

    return: FExpr[int32]
        The "nanosecond" part of the source column.


    Examples
    --------
    >>> from datetime import datetime as d
    >>> DT = dt.Frame([d(2020, 5, 11, 12, 0, 0), d(2021, 6, 14, 16, 10, 59, 394873)])
    >>> DT[:, {'time': f[0], 'ns': dt.time.nanosecond(f[0])}]
       | time                               ns
       | time64                          int32
    -- + --------------------------  ---------
     0 | 2020-05-11T12:00:00                 0
     1 | 2021-06-14T16:10:59.394873  394873000
    [2 rows x 2 columns]


    See Also
    --------
    - :func:`hour()` -- retrieve the "hour" component of a timestamp
    - :func:`minute()` -- retrieve the "minute" component of a timestamp
    - :func:`second()` -- retrieve the "second" component of a timestamp
