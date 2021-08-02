
.. xattr:: datatable.options.debug.arg_max_size
    :src: src/core/call_logger.cc get_arg_max_size set_arg_max_size
    :cvar: doc_options_debug_arg_max_size
    :settable: value

    This option limits the display size of each argument in order
    to prevent potentially huge outputs. It has no effect,
    if :attr:`debug.report_args <datatable.options.debug.report_args>` is
    `False`.

    Parameters
    ----------
    return: int
        Current `arg_max_size` value. Initially, this option is set to `100`.

    value: int
        New `arg_max_size` value, should be non-negative.
        If `value < 10`, then `arg_max_size` will be set to `10`.

    except: TypeError
        The exception is raised when `value` is negative.
