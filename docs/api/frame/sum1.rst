
.. xmethod:: datatable.Frame.sum1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_sum1
    :signature: sum1(self)

    Calculate the sum of all values for a one-column column frame and
    return it as a scalar.

    This function is a shortcut for::

        DT.sum()[0, 0]

    Parameters
    ----------
    return: None | float
        `None` is returned for non-numeric columns.

    except: ValueError
        If called on a Frame that has more or less than one column.

    See Also
    --------
    - :meth:`.sum()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.

    - :func:`dt.sum()` -- function for calculating the sum of all the values
      in a column or an expression; can also be applied per-group.

