
.. xmethod:: datatable.Frame.mean
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_mean
    :signature: mean(self)

    Calculate the mean value for each column in the frame.

    Parameters
    ----------
    return: Frame
        The frame will have one row and the same number/names
        of columns as in the current frame. All columns will have `float64`
        stype. For string/object columns this function returns NA values.

    See Also
    --------
    - :meth:`.mean1()` -- similar to this method, but operates on a
      single-column frame only, and returns a scalar value instead of
      a Frame.

    - :func:`dt.mean()` -- function for counting mean values in a column or
      an expression; can also be applied per-group.
