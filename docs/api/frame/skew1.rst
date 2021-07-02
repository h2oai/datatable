
.. xmethod:: datatable.Frame.skew1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_skew1
    :signature: skew1(self)

    Calculate the skewness for a one-column frame and return it as a scalar.

    This function is a shortcut for::

        DT.skew()[0, 0]

    Parameters
    ----------
    return: None | float
        `None` is returned for non-numeric columns.

    except: ValueError
        If called on a Frame that has more or less than one column.

    See Also
    --------
    - :meth:`.skew()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.
