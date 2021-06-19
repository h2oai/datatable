
.. xmethod:: datatable.Frame.min1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_min1
    :signature: min1(self)

    Find the smallest value in a single-column Frame. The frame's
    stype must be numeric.

    This function is a shortcut for::

        DT.min()[0, 0]

    Parameters
    ----------
    return: bool | int | float
        The returned value corresponds to the stype of the frame.

    except: ValueError
        If called on a Frame that has more or less than 1 column.

    See Also
    --------
    - :meth:`.min()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.

    - :func:`dt.min()` -- function for counting min values in a column or
      an expression; can also be applied per-group.
