
.. xattr:: datatable.options.debug.report_args
    :src: src/core/call_logger.cc get_report_args set_report_args
    :doc: doc_options_debug_report_args
    :settable: value

    This option controls whether log messages for the function
    and method calls should contain information about the arguments
    of those calls.

    Parameters
    ----------
    return: bool
        Current `report_args` value. Initially, this option is set to `False`.

    value: object
        New `report_args` value.
