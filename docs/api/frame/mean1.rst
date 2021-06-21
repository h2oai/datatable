
.. xmethod:: datatable.Frame.mean1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_mean1
    :signature: mean1(self)

    Calculate the mean value for a single-column Frame.

    This function is a shortcut for::

        DT.mean()[0, 0]

    Parameters
    ----------
    return: None | float
        `None` is returned for string/object columns.

    except: ValueError
        If called on a Frame that has more or less than one column.

    See Also
    --------
    - :meth:`.mean()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.

    - :func:`dt.mean()` -- function for calculatin mean values in a column or
      an expression; can also be applied per-group.
