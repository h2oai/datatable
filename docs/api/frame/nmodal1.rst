
.. xmethod:: datatable.Frame.nmodal1
    :src: src/core/frame/stats.cc Frame::_init_stats
    :tests: tests/test-dt-stats.py
    :cvar: doc_Frame_nmodal1
    :signature: nmodal1(self)

    Calculate the modal frequency for a single-column Frame.

    This function is a shortcut for::

        DT.nmodal()[0, 0]

    Parameters
    ----------
    return: int

    except: ValueError
        If called on a Frame that has more or less than one column.

    See Also
    --------
    - :meth:`.nmodal()` -- similar to this method, but can be applied to
      a Frame with an arbitrary number of columns.
