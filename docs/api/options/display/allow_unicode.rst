
.. xattr:: datatable.options.display.allow_unicode
    :src: src/core/frame/repr/repr_options.cc get_allow_unicode set_allow_unicode
    :cvar: doc_options_display_allow_unicode
    :settable: value

    This option controls whether or not unicode characters are
    allowed in the datatable output.

    Parameters
    ----------
    return: bool
        Current `allow_unicode` value. Initially, this option is set to `True`.

    value: bool
        New `allow_unicode` value. If `True`, datatable will allow unicode
        characters (encoded as UTF-8) to be printed into the output.
        If `False`, then unicode characters will either be avoided, or
        hex-escaped as necessary.
