
.. xmethod:: datatable.Frame.countna
    :src: src/core/frame/stats.cc Frame::_init_stats
    :cvar: doc_Frame_countna
    :tests: tests/test-dt-stats.py
    :signature: countna(self)

    Report the number of NA values in each column of the frame.

    Parameters
    ----------
    (return): Frame
        The frame will have one row and the same number/names of columns
        as in the current frame. All columns will have stype ``int64``.

    Examples
    --------
    .. code-block:: python

        >>> DT = dt.Frame(A=[1, 5, None], B=[math.nan]*3, C=[None, None, 'bah!'])
        >>> DT.countna()
           |     A      B      C
           | int64  int64  int64
        -- + -----  -----  -----
         0 |     1      3      2
        [1 row x 3 columns]

        >>> DT.countna().to_tuples()[0]
        >>> (1, 3, 2)


    See Also
    --------
    - :meth:`.countna1()` -- similar to this method, but operates on a
      single-column frame only, and returns a number instead of a Frame.

    - :func:`dt.count()` -- function for counting non-NA ("valid") values
      in a column; can also be applied per-group.
