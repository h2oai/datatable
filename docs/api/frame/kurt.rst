
.. xmethod:: datatable.Frame.kurt
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_kurt
    :signature: kurt(self)

    Calculate the excess kurtosis for each column in the frame.

    Parameters
    ----------
    return: Frame
        The frame will have one row and the same number/names
        of columns as in the current frame. All the columns
        will have `float64` stype. For non-numeric columns
        this function returns NA values.

    See Also
    --------
    - :meth:`.kurt1()` -- similar to this method, but operates on a
      single-column frame only, and returns a scalar value instead of
      a Frame.
