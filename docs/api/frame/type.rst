
.. xattr:: datatable.Frame.type
    :src: src/core/frame/py_frame.cc Frame::get_type
    :cvar: doc_Frame_type

    .. x-version-added:: v1.0.0

    The common :class:`dt.Type` for all columns.

    This property is well-defined only for frames where all columns have
    the same type.

    Parameters
    ----------
    return: Type | None
        For frames where all columns have the same type, this common
        type is returned. If a frame has 0 columns, `None` will be
        returned.

    except: InvalidOperationError
        This exception will be raised if the columns in the frame have
        different types.

    See also
    --------
    - :attr:`.types` -- list of types for all columns.
