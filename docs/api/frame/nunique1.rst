
.. xmethod:: datatable.Frame.nunique1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_nunique1
    :signature: nunique1(self)

    Count the number of unique values for a one-column frame and return it as a scalar.

    This function is a shortcut for::

        DT.nunique()[0, 0]

    Parameters
    ----------
    return: int

    except: ValueError
        If called on a Frame that has more or less than one column.

    See Also
    --------
    - :meth:`.nunique()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.
