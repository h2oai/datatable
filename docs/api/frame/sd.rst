
.. xmethod:: datatable.Frame.sd
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_sd
    :signature: sd(self)

    Calculate the standard deviation for each column in the frame.

    Parameters
    ----------
    return: Frame
        The frame will have one row and the same number/names
        of columns as in the current frame. All the columns
        will have `float64` stype. For non-numeric columns
        this function returns NA values.

    See Also
    --------
    - :meth:`.sd1()` -- similar to this method, but operates on a
      single-column frame only, and returns a scalar value instead of
      a Frame.


    - :func:`dt.sd()` -- function for calculating the standard deviation
      in a column or an expression; can also be applied per-group.

