
.. xmethod:: datatable.Frame.keys
    :src: src/core/frame/names.cc Frame::get_names

    Returns a tuple of column names, same as :attr:`.names`
    property.

    This method is not intended for public use. It is needed in order
    for :class:`dt.Frame` to satisfy Python's ``Mapping`` interface.
