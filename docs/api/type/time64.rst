
.. xattr:: datatable.Type.time64
    :src: --
    :tests: tests/types/test-time64.py

    .. x-version-added:: 1.0.0

    The ``time64`` type is used to represent a specific moment in time. This
    corresponds to ``datetime`` in Python, or ``timestamp`` in Arrow or pandas.
    Internally, this type is stored as a 64-bit integer containing the number of
    nanoseconds since the epoch (Jan 1, 1970) in UTC.

    This type is not `leap-seconds`_ aware, meaning that it assumes that each day
    has exactly 24×3600 seconds. In practice it means that calculating time
    difference between two ``time64`` moments may be off by the number of leap
    seconds that have occurred between them.

    Currently, ``time64`` type is not timezone-aware, addition of time zones is
    planned for the next release.

    ..
        A ``time64`` column may also carry a time zone as meta information. This time
        zone is used to convert the timestamp from the absolute UTC time to the local
        calendar. For example, suppose you have two ``time64`` columns: one is in UTC
        while the other is in *America/Los\_Angeles* time zone. Assume both columns
        store the same value ``1577836800000``. Then these two columns represent the
        same moment in time, however their calendar representations are different:
        ``2020-01-01T00:00:00Z`` and ``2019-12-31T16:00:00-0800`` respectively.

    A time64 column converts into ``datetime.datetime`` objects in python, a
    ``pa.timestamp('ns')`` type in pyarrow and ``dtype('datetime64[ns]')`` in
    numpy and pandas.


    Examples
    --------
    >>> DT = dt.Frame(["2018-01-31 03:16:57", "2021-06-15 15:44:23.951", None, "1965-11-25 19:29:00"])
    >>> DT[0] = dt.Type.time64
    >>> DT
       | C0
       | time64
    -- + -----------------------
     0 | 2018-01-31T03:16:57
     1 | 2021-06-15T15:44:23.951
     2 | NA
     3 | 1965-11-25T19:29:00
    [4 rows x 1 column]

    >>> dt.Type.time64.min
    datetime.datetime(1677, 9, 22, 0, 12, 43, 145225)
    >>> dt.Type.time64.max
    datetime.datetime(2262, 4, 11, 23, 47, 16, 854775)



.. _`leap-seconds`: https://en.wikipedia.org/wiki/Leap_second
