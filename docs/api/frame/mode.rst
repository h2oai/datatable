
.. xmethod:: datatable.Frame.mode
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_mode
    :signature: mode(self)

    Find the mode for each column in the frame.

    Parameters
    ----------
    return: Frame
        The frame will have one row and the same number/names
        of columns as in the current frame.

    See Also
    --------
    - :meth:`.mode1()` -- similar to this method, but operates on a
      single-column frame only, and returns a scalar value instead of
      a Frame.
