
.. xattr:: datatable.Frame.shape
    :src: src/core/frame/py_frame.cc Frame::get_shape
    :cvar: doc_Frame_shape

    Tuple with ``(nrows, ncols)`` dimensions of the frame.

    This property is read-only.

    Parameters
    ----------
    return: Tuple[int, int]
        Tuple with two integers: the first is the number of rows, the
        second is the number of columns.

    See also
    --------
    - :attr:`.nrows` -- getter for the number of rows;
    - :attr:`.ncols` -- getter for the number of columns.

