
.. xattr:: datatable.Frame.stype
    :src: src/core/frame/py_frame.cc Frame::get_stype
    :cvar: doc_Frame_stype

    .. x-version-deprecated:: 1.0.0

        This property is deprecated and will be removed in version 1.1.0.
        Please use :attr:`.types` instead.

    .. x-version-added:: 0.10.0

    The common :class:`dt.stype` for all columns.

    This property is well-defined only for frames where all columns have
    the same stype.

    Parameters
    ----------
    return: stype | None
        For frames where all columns have the same stype, this common
        stype is returned. If a frame has 0 columns, `None` will be
        returned.

    except: InvalidOperationError
        This exception will be raised if the columns in the frame have
        different stypes.

    See also
    --------
    - :attr:`.stypes` -- tuple of stypes for all columns.
