
.. xmethod:: datatable.Frame.to_csv
    :src: src/core/frame/to_csv.cc Frame::to_csv
    :tests: tests/frame/test-tocsv.py
    :cvar: doc_Frame_to_csv
    :signature: to_csv(self, path=None, *, sep =",", quoting="minimal",
           append=False, header="auto", bom=False, hex=False,
           compression=None, verbose=False, method="auto")

    Write the contents of the Frame into a CSV file.

    This method uses multiple threads to serialize the Frame's data. The
    number of threads is can be configured using the global option
    ``dt.options.nthreads``.

    The method supports simple writing to file, appending to an existing
    file, or creating a python string if no filename was provided.
    Optionally, the output could be gzip-compressed.


    Parameters
    ----------
    path: str
        Path to the output CSV file that will be created. If the file
        already exists, it will be overwritten. If no path is given,
        then the Frame will be serialized into a string, and that string
        will be returned.

    sep: str | ","
        Field separator for the output. It must be a single-character string.

    quoting: csv.QUOTE_* | "minimal" | "all" | "nonnumeric" | "none"
        `"minimal"` | `csv.QUOTE_MINIMAL`
            quote the string fields only as necessary, i.e. if the string
            starts or ends with the whitespace, or contains quote
            characters, separator, or any of the C0 control characters
            (including newlines, etc).

        `"all"` | `csv.QUOTE_ALL`
            all fields will be quoted, both string, numeric, and boolean.

        `"nonnumeric"` | `csv.QUOTE_NONNUMERIC`
            all string fields will be quoted.

        `"none"` | `csv.QUOTE_NONE`
            none of the fields will be quoted. This option must be used
            at user's own risk: the file produced may not be valid CSV.

    append: bool
        If True, the file given in the `path` parameter will be opened
        for appending (i.e. mode="a"), or created if it doesn't exist.
        If False (default), the file given in the `path` will be
        overwritten if it already exists.

    header: bool | "auto"
        This option controls whether or not to write headers into the
        output file. If this option is not given (or equal to ...), then
        the headers will be written unless the option `append` is True
        and the file `path` already exists. Thus, by default the headers
        will be written in all cases except when appending content into
        an existing file.

    bom: bool
        If True, then insert the byte-order mark into the output file
        (the option is False by default). Even if the option is True,
        the BOM will not be written when appending data to an existing
        file.

        According to Unicode standard, including BOM into text files is
        "neither required nor recommended". However, some programs (e.g.
        Excel) may not be able to recognize file encoding without this
        mark.

    hex: bool
        If True, then all floating-point values will be printed in hex
        format (equivalent to %a format in C `printf`). This format is
        around 3 times faster to write/read compared to usual decimal
        representation, so its use is recommended if you need maximum
        speed.

    compression: None | "gzip" | "auto"
        Which compression method to use for the output stream. The default
        is "auto", which tries to infer the compression method from the
        output file's name. The only compression format currently supported
        is "gzip". Compression may not be used when `append` is True.

    verbose: bool
        If True, some extra information will be printed to the console,
        which may help to debug the inner workings of the algorithm.

    method: "mmap" | "write" | "auto"
        Which method to use for writing to disk. On certain systems 'mmap'
        gives a better performance; on other OSes 'mmap' may not work at
        all.

    return: None | str | bytes
        None if `path` is non-empty. This is the most common case: the
        output is written to the file provided.

        String containing the CSV text as if it would have been written
        to a file, if the path is empty or None. If the compression is
        turned on, a bytes object will be returned instead.
