
.. xfunction:: datatable.iread
    :src: src/core/read/py_fread.cc iread
    :cvar: doc_dt_iread
    :signature: iread(
        anysource=None, *, file=None, text=None, cmd=None, url=None,
        columns=None, sep=None, dec=".", max_nrows=None, header=None,
        na_strings=None, verbose=False, fill=False, encoding=None,
        skip_to_string=None, skip_to_line=None, skip_blank_lines=False,
        strip_whitespace=True, quotechar='"',
        tempdir=None, nthreads=None, logger=None, errors="warn",
        memory_limit=None)

    This function is similar to :func:`fread()`, but allows reading
    multiple sources at once. For example, this can be used when the
    input is a list of files, or a glob pattern, or a multi-file archive,
    or multi-sheet XLSX file, etc.


    Parameters
    ----------
    ...: ...
        Most parameters are the same as in :func:`fread()`. All parse
        parameters will be applied to all input files.

    errors: "warn" | "raise" | "ignore" | "store"
        What action to take when one of the input sources produces an
        error. Possible actions are: `"warn"` -- each error is converted
        into a warning and emitted to user, the source that produced the
        error is then skipped; `"raise"` -- the errors are raised
        immediately and the iteration stops; `"ignore"` -- the erroneous
        sources are silently ignored; `"store"` -- when an error is
        raised, it is captured and returned to the user, then the iterator
        continues reading the subsequent sources.

    (return): Iterator[Frame] | Iterator[Frame|Exception]
        The returned object is an iterator that produces :class:`Frame` s.
        The iterator is lazy: each frame is read only as needed, after the
        previous frame was "consumed" by the user. Thus, the user can
        interrupt the iterator without having to read all the frames.

        Each :class:`Frame` produced by the iterator has a ``.source``
        attribute that describes the source of each frame as best as
        possible. Each source depends on the type of the input: either a
        file name, or a URL, or the name of the file in an archive, etc.

        If the `errors` parameter is `"store"` then the iterator may
        produce either Frames or exception objects.


    See Also
    --------
    - :func:`fread()`
