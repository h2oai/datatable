
.. xattr:: datatable.Type.date32
    :src: --

    .. x-version-added:: 1.0.0

    The ``date32`` type represents a particular calendar date without a time
    component. Internally, this type is stored as a 32-bit signed integer
    counting the number of days since 1970-01-01 ("the epoch"). Thus, this
    type accommodates dates within the range of approximately ±5.8 million
    years.

    The calendar used for this type is `proleptic Gregorian`_, meaning that it
    extends the modern-day Gregorian calendar into the past before this calendar
    was first adopted, and into the future, long after it will have been
    replaced.

    This type corresponds to ``datetime.date`` in Python, ``pa.date32()`` in
    pyarrow, and ``np.dtype('<M8[D]')`` in numpy.

    .. note::

        Python's ``datetime.date`` object can accommodate dates from year 1 to
        year 9999, which is much smaller than what our ``date32`` type allows.
        As a consequence, when ``date32`` values that are outside of year range
        1-9999 are converted to python, they become integers instead of
        ``datetime.date`` objects.

        For the same reason the :attr:`.min` and :attr:`.max` properties of this
        type also return integers.

    .. _`proleptic gregorian`: https://en.wikipedia.org/wiki/Proleptic_Gregorian_calendar


    Examples
    --------
    >>> from datetime import date
    >>> DT = dt.Frame([date(2020, 1, 30), date(2020, 3, 11), None, date(2021, 6, 15)])
    >>> DT.type
    Type.date32
    >>> DT
       | C0
       | date32
    -- + ----------
     0 | 2020-01-30
     1 | 2020-03-11
     2 | NA
     3 | 2021-06-15
    [4 rows x 1 column]

    >>> dt.Type.date32.min
    -2147483647
    >>> dt.Type.date32.max
    2146764179
    >>> dt.Frame([dt.Type.date32.min, date(2021, 6, 15), dt.Type.date32.max], stype='date32')
       | C0
       | date32
    -- + --------------
     0 | -5877641-06-24
     1 | 2021-06-15
     2 | 5879610-09-09
    [3 rows x 1 column]

