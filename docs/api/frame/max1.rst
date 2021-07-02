
.. xmethod:: datatable.Frame.max1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_max1
    :signature: max1(self)

    Return the largest value in a single-column Frame. The frame's
    stype must be numeric.

    This function is a shortcut for::

        DT.max()[0, 0]

    Parameters
    ----------
    return: bool | int | float
        The returned value corresponds to the stype of the frame.

    except: ValueError
        If called on a Frame that has more or less than one column.

    See Also
    --------
    - :meth:`.max()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.

    - :func:`dt.max()` -- function for counting max values in a column or
      an expression; can also be applied per-group.
