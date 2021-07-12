
.. xattr:: datatable.Frame.ncols
    :src: src/core/frame/py_frame.cc Frame::get_ncols
    :cvar: doc_Frame_ncols

    Number of columns in the frame.

    Parameters
    ----------
    return: int
        The number of columns can be either zero or a positive integer.

    Notes
    -----
    The expression `len(DT)` also returns the number of columns in the
    frame `DT`. Such usage, however, is not recommended.

    See also
    --------
    - :attr:`.nrows`: getter for the number of rows of the frame.
