
.. xmethod:: datatable.Frame.sd1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_sd1
    :signature: sd1(self)

    Calculate the standard deviation for a one-column frame and
    return it as a scalar.

    This function is a shortcut for::

        DT.sd()[0, 0]

    Parameters
    ----------
    return: None | float
        `None` is returned for non-numeric columns.

    except: ValueError
        If called on a Frame that has more or less than one column.

    See Also
    --------
    - :meth:`.sd()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.

    - :func:`dt.sd()` -- function for calculating the standard deviation
      in a column or an expression; can also be applied per-group.
