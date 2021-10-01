
.. xattr:: datatable.options.display.interactive
    :src: src/core/frame/repr/repr_options.cc get_interactive set_interactive
    :cvar: doc_options_display_interactive
    :settable: value

    ..  warning::

        This option is currently not working properly
        `[#2669] <https://github.com/h2oai/datatable/issues/2669>`_

    This option controls the behavior of a Frame when it is viewed in a
    text console. To enter the interactive mode manually, one can still
    call the :meth:`Frame.view() <dt.Frame.view>` method.

    Parameters
    ----------
    return: bool
        Current `interactive` value. Initially, this option is set to `False`.

    value: bool
        New `interactive` value. If `True`, frames will be shown in
        the interactove mode, allowing you to navigate the rows/columns
        with the keyboard. If `False`, frames will be shown in regular,
        non-interactive mode.
