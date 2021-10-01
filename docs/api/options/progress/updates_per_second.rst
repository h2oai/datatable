
.. xattr:: datatable.options.progress.updates_per_second
    :src: src/core/progress/_options.cc get_updates_per_second set_updates_per_second
    :cvar: doc_options_progress_updates_per_second
    :settable: value

    This option controls the progress bar update frequency.


    Parameters
    ----------
    return: float
        Current `updates_per_second` value. Initially, this option is set to `25.0`.

    value: float
        New `updates_per_second` value. This is the number of times per second
        the display of the progress bar should be updated.

