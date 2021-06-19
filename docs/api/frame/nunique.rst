
.. xmethod:: datatable.Frame.nunique
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_nunique
    :signature: nunique(self)

    Count the number of unique values for each column in the frame.

    Parameters
    ----------
    return: Frame
        The frame will have one row and the same number/names
        of columns as in the current frame. All the columns
        will have `int64` stype.

    See Also
    --------
    - :meth:`.nunique1()` -- similar to this method, but operates on a
      single-column frame only, and returns a scalar value instead of
      a Frame.
