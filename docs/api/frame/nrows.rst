
.. xattr:: datatable.Frame.nrows
    :src: src/core/frame/py_frame.cc Frame::get_nrows Frame::set_nrows
    :cvar: doc_Frame_nrows
    :settable: n

    Number of rows in the Frame.

    Assigning to this property will change the height of the Frame,
    either by truncating if the new number of rows is smaller than the
    current, or filling with NAs if the new number of rows is greater.

    Increasing the number of rows of a keyed Frame is not allowed.

    Parameters
    ----------
    return: int
        The number of rows can be either zero or a positive integer.

    n: int
        The new number of rows for the frame, this should be a non-negative
        integer.

    See also
    --------
    - :attr:`.ncols`: getter for the number of columns of the frame.
