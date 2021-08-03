
.. xattr:: datatable.options.debug.logger
    :src: src/core/call_logger.cc get_logger set_logger
    :cvar: doc_options_debug_logger
    :settable: value

    The logger object used for reporting calls to datatable core
    functions. This option has no effect if
    :attr:`debug.enabled <datatable.options.debug.enabled>` is `False`.

    Parameters
    ----------
    return: object
        Current `logger` value. Initially, this option is set to `None`,
        meaning that the built-in logger should be used.

    value: object
        New `logger` value.

    except: TypeError
        The exception is raised when `value` is not an object
        having a method `.debug(self, msg)`.
