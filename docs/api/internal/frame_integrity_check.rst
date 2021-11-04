
.. xfunction:: datatable.internal.frame_integrity_check
    :src: src/core/datatablemodule.cc frame_integrity_check
    :cvar: doc_internal_frame_integrity_check
    :signature: frame_integrity_check(frame)

    This function performs a range of tests on the `frame` to verify
    that its internal state is consistent. It returns None on success,
    or throws an ``AssertionError`` if any problems were found.


    Parameters
    ----------
    frame: Frame
        A :class:`dt.Frame` object that needs to be checked for internal consistency.

    return: None

    except: AssertionError
        An exception is raised if there were any issues with the `frame`.
