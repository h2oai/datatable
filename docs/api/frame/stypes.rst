
.. xattr:: datatable.Frame.stypes
    :src: src/core/frame/py_frame.cc Frame::get_stypes
    :cvar: doc_Frame_stypes

    .. x-version-deprecated:: 1.0.0

        This property is deprecated and will be removed in version 1.2.0.
        Please use :attr:`.types` instead.

    The tuple of each column's stypes ("storage types").

    Parameters
    ----------
    return: Tuple[stype, ...]
        The length of the tuple is the same as the number of columns in
        the frame.

    See also
    --------
    - :attr:`.stype` -- common stype for all columns
    - :attr:`.ltypes` -- tuple of columns' logical types
