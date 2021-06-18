
.. xattr:: datatable.Frame.types
    :src: src/core/frame/py_frame.cc Frame::get_types
    :cvar: doc_Frame_types

    .. x-version-added:: v1.0.0

    The list of `Type`s for each column of the frame.

    Parameters
    ----------
    return: List[Type]
        The length of the list is the same as the number of columns in the frame.


    See also
    --------
    - :attr:`.stypes` -- old interface for column types
