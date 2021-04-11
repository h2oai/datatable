
.. xattr:: datatable.Type.time64
    :src: --

    .. x-version-added:: 1.0.0

    The ``time64`` type is used to represent a specific moment in time. This
    corresponds to ``datetime`` in Python, or ``timestamp`` in Arrow or pandas.
    Internally, this type is stored as a 64-bit integer containing the number of
    nanoseconds since the epoch (Jan 1, 1970) in UTC.

    This type is not `leap-seconds`_ aware, meaning that it assumes that each day
    has exactly 24×3600 seconds. In practice it means that calculating time
    difference between two ``time64`` moments may be off by the number of leap
    seconds that have occurred between them.

    A ``time64`` column may also carry a time zone as meta information. This time
    zone is used to convert the timestamp from the absolute UTC time to the local
    calendar. For example, suppose you have two ``time64`` columns: one is in UTC
    while the other is in *America/Los\_Angeles* time zone. Assume both columns
    store the same value ``1577836800000``. Then these two columns represent the
    same moment in time, however their calendar representations are different:
    ``2020-01-01T00:00:00Z`` and ``2019-12-31T16:00:00-0800`` respectively.


.. _`leap-seconds`: https://en.wikipedia.org/wiki/Leap_second
