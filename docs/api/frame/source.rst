
.. xattr:: datatable.Frame.source
    :src: src/core/frame/py_frame.cc Frame::get_source
    :cvar: doc_Frame_source

    .. x-version-added:: 0.11

    The name of the file where this frame was loaded from.

    This is a read-only property that describes the origin of the frame.
    When a frame is loaded from a Jay or CSV file, this property will
    contain the name of that file. Similarly, if the frame was opened
    from a URL or a from a shell command, the source will report the
    original URL / the command.

    Certain sources may be converted into a Frame only partially,
    in such case the ``source`` property will attempt to reflect this
    fact. For example, when opening a multi-file zip archive, the
    source will contain the name of the file within the archive.
    Similarly, when opening an XLS file with several worksheets, the
    source property will contain the name of the XLS file, the name of
    the worksheet, and possibly even the range of cells that were read.

    Parameters
    ----------
    return: str | None
        If the frame was loaded from a file or similar resource, the
        name of that file is returned. If the frame was computed, or its
        data modified, the property will return ``None``.
