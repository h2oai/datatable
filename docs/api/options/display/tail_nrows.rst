
.. xattr:: datatable.options.display.tail_nrows
    :src: src/core/frame/repr/repr_options.cc get_tail_nrows set_tail_nrows
    :cvar: doc_options_display_tail_nrows
    :settable: value

    This option controls the number of rows from the bottom of a frame
    to be displayed when the frame's output is truncated due to the total number of
    rows exceeding :attr:`max_nrows <datatable.options.display.max_nrows>`
    value.

    Parameters
    ----------
    return: int
        Current `tail_nrows` value. Initially, this option is set to `5`.

    value: int
        New `tail_nrows` value, should be non-negative.

    except: ValueError
        The exception is raised when the `value` is negative.
