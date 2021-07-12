
.. xattr:: datatable.Frame.meta
    :src: src/core/frame/py_frame.cc Frame::get_meta Frame::set_meta
    :settable: new_meta
    :cvar: doc_Frame_meta

    .. x-version-added:: 1.0

    Frame's meta information.

    This property contains meta information, if any, as set by datatable
    functions and methods. It is a settable property, so that users can also
    update it with any information relevant to a particular frame.

    It is not guaranteed that the existing meta information will be preserved
    by the functions and methods called on the frame. In particular,
    it is not preserved when exporting data into a Jay file or pickling the data.
    This behavior may change in the future.

    The default value for this property is `None`.


    Parameters
    ----------
    return: dict | None
        If the frame carries any meta information, the corresponding meta
        information dictionary is returned, `None` is returned otherwise.

    new_meta: dict | None
        New meta information.
