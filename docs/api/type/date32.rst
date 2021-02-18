
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
    was first adopted.

    This type corresponds to ``datetime.date`` in Python, ``pa.date32()`` in
    pyarrow, and ``np.dtype('<M8[D]')`` in numpy.


    .. _`proleptic gregorian`: https://en.wikipedia.org/wiki/Proleptic_Gregorian_calendar


