
.. xattr:: datatable.options.frame.names_auto_prefix
    :src: src/core/frame/names.cc get_names_auto_prefix set_names_auto_prefix
    :settable: new_names_auto_prefix
    :cvar: doc_options_frame_names_auto_prefix

    This option controls the prefix that is used for auto-naming
    the columns. By default, the names that datatable assigns to frame's columns are
    `C0`, `C1`, `C2`, etc. Setting `names_auto_prefix`, for instance, to
    `Z` will cause the columns to be named as `Z1`, `Z2`, `Z3`, etc.

    Parameters
    ----------
    return: str
        Current `names_auto_prefix` value. Initially, this option is set to `C`.

    new_names_auto_prefix: str
        New `names_auto_prefix` value.

    See Also
    --------
    - :ref:`name-mangling` -- tutorial on name mangling.

