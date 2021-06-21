
.. xmethod:: datatable.Frame.sum
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_sum
    :signature: sum(self)

    Calculate the sum of all values for each column in the frame.

    Parameters
    ----------
    return: Frame
        The frame will have one row and the same number/names
        of columns as in the current frame. All the columns
        will have `float64` stype. For non-numeric columns
        this function returns NA values.

    See Also
    --------
    - :meth:`.sum1()` -- similar to this method, but operates on a
      single-column frame only, and returns a scalar value instead of
      a Frame.


    - :func:`dt.sum()` -- function for calculating the sum of all the values
      in a column or an expression; can also be applied per-group.
