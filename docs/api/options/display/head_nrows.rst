
.. xattr:: datatable.options.display.head_nrows
    :src: src/core/frame/repr/repr_options.cc get_head_nrows set_head_nrows
    :cvar: doc_options_display_head_nrows
    :settable: value

    This option controls the number of rows from the top of a frame
    to be displayed when the frame's output is truncated due to the total number of
    rows exceeding :attr:`display.max_nrows <datatable.options.display.max_nrows>`
    value.

    Parameters
    ----------
    return: int
        Current `head_nrows` value. Initially, this option is set to `15`.

    value: int
        New `head_nrows` value, should be non-negative.

    except: ValueError
        The exception is raised when the `value` is negative.
