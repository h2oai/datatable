
.. xattr:: datatable.options.progress.enabled
    :src: src/core/progress/_options.cc get_enabled set_enabled
    :cvar: doc_options_progress_enabled
    :settable: value

    This option controls if the progress reporting is enabled.

    Parameters
    ----------
    return: bool
        Current `enabled` value. Initially, this option is set to `True`
        if the `stdout` is connected to a terminal or a Jupyter Notebook,
        and `False` otherwise.

    value: bool
        New `enabled` value. If `True`, the progress reporting
        functionality will be turned on. If `False`, it is turned off.

