
.. xattr:: datatable.Frame.ltypes
    :src: src/core/frame/py_frame.cc Frame::get_ltypes
    :cvar: doc_Frame_ltypes

    .. x-version-deprecated:: 1.0.0

        Use property :attr:`.types` instead.

    The tuple of each column's ltypes ("logical types").

    Parameters
    ----------
    return: Tuple[ltype, ...]
        The length of the tuple is the same as the number of columns in
        the frame.

    See also
    --------
    - :attr:`.stypes` -- tuple of columns' storage types
