
.. xfunction:: datatable.internal.frame_columns_virtual
    :src: src/core/datatablemodule.cc frame_columns_virtual
    :cvar: doc_internal_frame_columns_virtual
    :signature: frame_columns_virtual(frame)

    .. x-version-deprecated:: 0.11.0

    Return the list indicating which columns in the `frame` are virtual.

    Parameters
    ----------
    return: List[bool]
        Each element in the list indicates whether the corresponding column
        is virtual or not.

    Notes
    -----

    This function will be expanded and moved into the main :class:`dt.Frame` class.
