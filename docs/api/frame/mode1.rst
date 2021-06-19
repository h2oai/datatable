
.. xmethod:: datatable.Frame.mode1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_mode1
    :signature: mode1(self)

    Find the mode for a single-column Frame.

    This function is a shortcut for::

        DT.mode()[0, 0]

    Parameters
    ----------
    return: bool | int | float | str | object
        The returned value corresponds to the stype of the column.

    except: ValueError
        If called on a Frame that has more or less than one column.

    See Also
    --------
    - :meth:`.mode()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.

