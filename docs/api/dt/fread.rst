
.. xfunction:: datatable.fread
    :src: src/core/read/py_fread.cc fread
    :cvar: doc_dt_fread
    :signature: fread(
        anysource=None, *, file=None, text=None, cmd=None, url=None,
        columns=None, sep=None, dec=".", max_nrows=None, header=None,
        na_strings=None, verbose=False, fill=False, encoding=None,
        skip_to_string=None, skip_to_line=0, skip_blank_lines=False,
        strip_whitespace=True, quotechar='"', tempdir=None,
        nthreads=None, logger=None, multiple_sources="warn",
        memory_limit=None)

    This function is capable of reading data from a variety of input formats,
    producing a :class:`Frame` as the result. The recognized formats are:
    CSV, Jay, XLSX, and plain text. In addition, the data may be inside an
    archive such as ``.tar``, ``.gz``, ``.zip``, ``.gz2``, and ``.tgz``.


    Parameters
    ----------
    anysource: str | bytes | file | Pathlike | List
        The first (unnamed) argument to fread is the *input source*.
        Multiple types of sources are supported, and they can be named
        explicitly: `file`, `text`, `cmd`, and `url`. When the source is
        not named, fread will attempt to guess its type. The most common
        type is `file`, but sometimes the argument is resolved as `text`
        (if the string contains newlines) or `url` (if the string starts
        with `https://` or similar).

        Only one argument out of `anysource`, `file`, `text`, `cmd` or
        `url` can be specified at once.

    file: str | file | Pathlike
        A file source can be either the name of the file on disk, or a
        python "file-like" object -- i.e. any object having method
        ``.read()``.

        Generally, specifying a file name should be preferred, since
        reading from a Python ``file`` can only be done in single-threaded
        mode.

        This argument also supports addressing files inside an archive,
        or sheets inside an Excel workbook. Simply write the name of the
        file as if the archive was a folder: `"data.zip/train.csv"`.

    text: str | bytes
        Instead of reading data from file, this argument provides the data
        as a simple in-memory blob.

    cmd: str
        A command that will be executed in the shell and its output then
        read as text.

    url: str
        This parameter can be used to specify the URL of the input file.
        The data will first be downloaded into a temporary directory and
        then read from there. In the end the temporary files will be
        removed.

        We use the standard ``urllib.request`` module to download the
        data. Changing the settings of that module, for example installing
        proxy, password, or cookie managers will allow you to customize
        the download process.

    columns: ...
        Limit which columns to read from the input file.

    sep: str | None
        Field separator in the input file. If this value is `None`
        (default) then the separator will be auto-detected. Otherwise it
        must be a single-character string. When ``sep='\n'``, then the
        data will be read in *single-column* mode. Characters
        ``["'`0-9a-zA-Z]`` are not allowed as the separator, as well as
        any non-ASCII characters.

    dec: "." | ","
        Decimal point symbol for floating-point numbers.

    max_nrows: int
        The maximum number of rows to read from the file. Setting this
        parameter to any negative number is equivalent to have no limit
        at all. Currently this parameter doesn't always work correctly.

    header: bool | None
        If `True` then the first line of the CSV file contains the header.
        If `False` then there is no header. By default the presence of the
        header is heuristically determined from the contents of the file.

    na_strings: List[str]
        The list of strings that were used in the input file to represent
        NA values.

    fill: bool
        If `True` then the lines of the CSV file are allowed to have
        uneven number of fields. All missing fields will be filled with
        NAs in the resulting frame.

    encoding: str | None
        If this parameter is provided, then the input will be recoded
        from this encoding into UTF-8 before reading. Any encoding
        registered with the python ``codec`` module can be used.

    skip_to_string: str | None
        Start reading the file from the line containing this string. All
        previous lines will be skipped and discarded. This parameter
        cannot be used together with `skip_to_line`.

    skip_to_line: int
        If this setting is given, then this many lines in the file will
        be skipped before we start to parse the file. This can be used
        for example when several first lines in the file contain non-CSV
        data and therefore must be skipped. This parameter cannot be
        used together with `skip_to_string`.

    skip_blank_lines: bool
        If `True`, then any empty lines in the input will be skipped. If
        this parameter is `False` then: (a) in single-column mode empty
        lines are kept as empty lines; otherwise (b) if `fill=True` then
        empty lines produce a single line filled with NAs in the output;
        otherwise (c) an :exc:`dt.exceptions.IOError` is raised.

    strip_whitespace: bool
        If `True`, then the leading/trailing whitespace will be stripped
        from unquoted string fields. Whitespace is always skipped from
        numeric fields.

    quotechar: '"' | "'" | "`"
        The character that was used to quote fields in the CSV file. By
        default the double-quote mark `'"'` is assumed.

    tempdir: str | None
        Use this directory for storing temporary files as needed. If not
        provided then the system temporary directory will be used, as
        determined via the :ext-mod:`tempfile` Python module.

    nthreads: int | None
        Number of threads to use when reading the file. This number cannot
        exceed the number of threads in the pool ``dt.options.nthreads``.
        If `0` or negative number of threads is requested, then it will be
        treated as that many threads less than the maximum. By default
        all threads in the thread pool are used.

    verbose: bool
        If `True`, then print detailed information about the internal
        workings of fread to stdout (or to `logger` if provided).

    logger: object
        Logger object that will receive verbose information about fread's
        progress. When this parameter is specified, `verbose` mode will
        be turned on automatically.

    multiple_sources: "warn" | "error" | "ignore"
        Action that should be taken when the input resolves to multiple
        distinct sources. By default, (`"warn"`) a warning will be issued
        and only the first source will be read and returned as a Frame.
        The `"ignore"` action is similar, except that the extra sources
        will be discarded without a warning. Lastly, an :exc:`dt.exceptions.IOError`
        can be raised if the value of this parameter is `"error"`.

        If you want all sources to be read instead of only the first one
        then consider using :func:`iread()`.

    memory_limit: int
        Try not to exceed this amount of memory allocation (in bytes)
        when reading the data. This limit is advisory and not enforced
        very strictly.

        This setting is useful when reading data from a file that is
        substantially larger than the amount of RAM available on your
        machine.

        When this parameter is specified and fread sees that it needs
        more RAM than the limit in order to read the input file, then
        it will dump the data that was read so far into a temporary file
        in binary format. In the end the returned Frame will be partially
        composed from data located on disk, and partially from the data
        in memory. It is advised to either store this data as a Jay file
        or filter and materialize the frame (if not the performance may
        be slow).

    return: Frame
        A single :class:`Frame` object is always returned.

        .. x-version-changed:: 0.11.0

            Previously, a ``dict`` of Frames was returned when multiple
            input sources were provided.

    except: dt.exceptions.IOError

    See Also
    --------
    - :func:`iread()`
    - :ref:`Fread Examples` user guide for usage examples.
