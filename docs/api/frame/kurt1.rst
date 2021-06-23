
.. xmethod:: datatable.Frame.kurt1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_kurt1
    :signature: kurt1(self)

    Calculate the excess kurtosis for a one-column frame and
    return it as a scalar.

    This function is a shortcut for::

        DT.kurt()[0, 0]

    Parameters
    ----------
    return: None | float
        `None` is returned for non-numeric columns.

    except: ValueError
        If called on a Frame that has more or less than one column.

    See Also
    --------
    - :meth:`.kurt()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.
