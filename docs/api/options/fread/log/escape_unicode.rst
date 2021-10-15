
.. xattr:: datatable.options.fread.log.escape_unicode
    :src: src/core/csv/reader.cc get_escape_unicode set_escape_unicode
    :cvar: doc_options_fread_log_escape_unicode
    :settable: value

    This option controls escaping of the unicode characters.

    Use this option if your terminal cannot print unicode,
    or if the output gets somehow corrupted because of the unicode characters.

    Parameters
    ----------
    return: bool
        Current `escape_unicode` value. Initially, this option is set to `False`.

    value: bool
        If `True`, all unicode characters in the verbose log will be written
        in hexadecimal notation. If `False`, no escaping of the unicode
        characters will be performed.
