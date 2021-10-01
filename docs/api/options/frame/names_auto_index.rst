
.. xattr:: datatable.options.frame.names_auto_index
    :src: src/core/frame/names.cc get_names_auto_index set_names_auto_index
    :settable: new_names_auto_index
    :cvar: doc_options_frame_names_auto_index

    This option controls the starting index that is used for auto-naming
    the columns. By default, the names that datatable assigns to frame's columns are
    `C0`, `C1`, `C2`, etc. Setting `names_auto_index`, for instance, to
    `1` will cause the columns to be named as `C1`, `C2`, `C3`, etc.

    Parameters
    ----------
    return: int
        Current `names_auto_index` value. Initially, this option is set to `0`.

    new_names_auto_index: int
        New `names_auto_index` value.

    See Also
    --------
    - :ref:`name-mangling` -- tutorial on name mangling.
