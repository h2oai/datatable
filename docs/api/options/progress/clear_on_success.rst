
.. xattr:: datatable.options.progress.clear_on_success
    :src: src/core/progress/_options.cc get_clear_on_success set_clear_on_success
    :cvar: doc_options_progress_clear_on_success
    :settable: value

    This option controls if the progress bar is cleared on success.


    Parameters
    ----------
    return: bool
        Current `clear_on_success` value. Initially, this option is set to `False`.

    value: bool
        New `clear_on_success` value. If `True`, the progress bar is cleared when
        job finished successfully. If `False`, the progress remains visible
        even when the job has already finished.

