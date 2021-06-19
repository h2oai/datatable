
.. xmethod:: datatable.Frame.min
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_min
    :signature: min(self)

    Find the smallest value in each column of the frame.

    Parameters
    ----------
    return: Frame
        The frame will have one row and the same number, names and stypes
        of columns as in the current frame. For string/object columns
        this function returns NA values.

    See Also
    --------
    - :meth:`.min1()` -- similar to this method, but operates on a
      single-column frame only, and returns a scalar value instead of
      a Frame.

    - :func:`dt.min()` -- function for counting min values in a column or
      an expression; can also be applied per-group.
