
.. xmethod:: datatable.Frame.sum
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_sum
    :signature: sum(self)

    Calculate the sum of all values for each column in the frame.

    Parameters
    ----------
    return: Frame
        The resulting frame will have one row and the same
        number/names of columns as in the original frame.
        The column types are :attr:`int64 <dt.Type.int64>` for
        void, boolean and integer columns, :attr:`float32 <dt.Type.float32>`
        for :attr:`float32 <dt.Type.float32>` columns and
        :attr:`float64 <dt.Type.float64>` for :attr:`float64 <dt.Type.float64>`
        columns. For non-numeric columns an NA :attr:`float64 <dt.Type.float64>`
        column is returned.

    See Also
    --------
    - :meth:`.sum1()` -- similar to this method, but operates on a
      single-column frame only, and returns a scalar value instead of
      a Frame.

    - :func:`dt.sum()` -- function for calculating the sum of all the values
      in a column or an expression; can also be applied per-group.
