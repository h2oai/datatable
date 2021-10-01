
.. xattr:: datatable.options.display.max_column_width
    :src: src/core/frame/repr/repr_options.cc get_max_column_width set_max_column_width
    :cvar: doc_options_display_max_column_width
    :settable: value

    This option controls the threshold for the column's width
    to be truncated. If a column's name or its values exceed
    the `max_column_width`, the content of the column is truncated
    to `max_column_width` characters when printed.

    This option applies to both the rendering of a frame in a terminal,
    and the rendering in a Jupyter notebook.

    Parameters
    ----------
    return: int
        Current `max_column_width` value. Initially, this option is set to `100`.

    value: int
        New `max_column_width` value, cannot be less than `2`.
        If `value` equals to `None`, the column's content
        would never be truncated.

    except: ValueError
        The exception is raised when the `value` is less than `2`.
