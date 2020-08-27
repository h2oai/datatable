.. py:currentmodule:: datatable

.. xmethod:: datatable.Frame.keys
    :src: src/core/frame/names.cc Frame::get_names

    Returns a tuple of column names, same as :data:`.names` property.

    This method is not intended for public use. It is needed in order
    for :class:`Frame` to satisfy Python's ``Mapping`` interface.
