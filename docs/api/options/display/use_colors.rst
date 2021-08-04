
.. xattr:: datatable.options.display.use_colors
    :src: src/core/frame/repr/repr_options.cc get_use_colors set_use_colors
    :cvar: doc_options_display_use_colors
    :settable: value

    This option controls whether or not to use colors when printing
    datatable messages into the console. Turn this off if your terminal is unable to
    display ANSI escape sequences, or if the colors make output not legible.

    Parameters
    ----------
    return: bool
        Current `use_colors` value. Initially, this option is set to `True`.

    value: bool
        New `use_colors` value.

