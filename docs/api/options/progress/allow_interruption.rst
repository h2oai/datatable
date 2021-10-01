
.. xattr:: datatable.options.progress.allow_interruption
    :src: src/core/progress/_options.cc get_allow_interruption set_allow_interruption
    :cvar: doc_options_progress_allow_interruption
    :settable: value

    This option controls if the datatable tasks could be interrupted.


    Parameters
    ----------
    return: bool
        Current `allow_interruption` value. Initially, this option is set to `True`.

    value: bool
        New `allow_interruption` value. If `True`, datatable will be allowed
        to handle the `SIGINT` signal to interrupt long-running tasks.
        If `False`, it will not be possible to interrupt tasks with `SIGINT`.
