
.. xattr:: datatable.options.display.max_nrows
    :src: src/core/frame/repr/repr_options.cc get_max_nrows set_max_nrows
    :cvar: doc_options_display_max_nrows
    :settable: value

    This option controls the threshold for the number of rows
    in a frame to be truncated when printed to the console.

    If a frame has more rows than `max_nrows`, it will be displayed
    truncated: only its first
    :attr:`head_nrows <datatable.options.display.head_nrows>`
    and last
    :attr:`tail_nrows <datatable.options.display.tail_nrows>`
    rows will be printed. Otherwise, no truncation will occur.
    It is recommended to have `head_nrows + tail_nrows <= max_nrows`.

    Parameters
    ----------
    return: int
        Current `max_nrows` value. Initially, this option is set to `30`.

    value: int
        New `max_nrows` value. If this option is set to `None` or
        to a negative value, no frame truncation will occur when printed,
        which may cause the console to become unresponsive for frames
        with large number of rows.
