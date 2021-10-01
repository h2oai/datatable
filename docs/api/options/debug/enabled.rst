
.. xattr:: datatable.options.debug.enabled
    :src: src/core/call_logger.cc get_enabled set_enabled
    :cvar: doc_options_debug_enabled
    :settable: value

    This option controls whether or not all the calls to the datatable core
    functions should be logged.


    Parameters
    ----------
    return: bool
        Current `enabled` value. Initially, this option is set to `False`.

    value: bool
        New `enabled` value. If set to `True`, all the calls to the datatable
        core functions will be logged along with their respective timings.
