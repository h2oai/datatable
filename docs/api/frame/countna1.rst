
.. xmethod:: datatable.Frame.countna1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_countna1
    :signature: countna1(self)

    Return the number of NA values in a single-column Frame.

    This function is a shortcut for::

        DT.countna()[0, 0]

    Parameters
    ----------
    (except): ValueError
        If called on a Frame that has more or less than one column.

    (return): int

    See Also
    --------
    - :meth:`.countna()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.

    - :func:`dt.count()` -- function for counting non-NA ("valid") values
      in a column; can also be applied per-group.
