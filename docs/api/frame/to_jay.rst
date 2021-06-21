
.. xmethod:: datatable.Frame.to_jay
    :src: src/core/jay/save_jay.cc Frame::to_jay
    :cvar: doc_Frame_to_jay
    :signature: to_jay(self, path=None, method='auto')

    Save this frame to a binary file on disk, in `.jay` format.

    Parameters
    ----------
    path: str | None
        The destination file name. Although not necessary, we recommend
        using extension ".jay" for the file. If the file exists, it will
        be overwritten.
        If this argument is omitted, the file will be created in memory
        instead, and returned as a `bytes` object.

    method: 'mmap' | 'write' | 'auto'
        Which method to use for writing the file to disk. The "write"
        method is more portable across different operating systems, but
        may be slower. This parameter has no effect when `path` is
        omitted.

    return: None | bytes
        If the `path` parameter is given, this method returns nothing.
        However, if `path` was omitted, the return value is a `bytes`
        object containing encoded frame's data.
