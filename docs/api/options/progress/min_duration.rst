
.. xattr:: datatable.options.progress.min_duration
    :src: src/core/progress/_options.cc get_min_duration set_min_duration
    :cvar: doc_options_progress_min_duration
    :settable: value

    This option controls the minimum duration of a task to show the progress bar.


    Parameters
    ----------
    return: float
        Current `min_duration` value. Initially, this option is set to `0.5`.

    value: float
        New `min_duration` value. The progress bar will not be shown
        if the duration of an operation is smaller than `value`.
        If this value is non-zero, then the progress bar will only be shown
        for long-running operations, whose duration (estimated or actual)
        exceeds this threshold.
