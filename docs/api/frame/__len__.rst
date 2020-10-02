
.. xmethod:: datatable.Frame.__len__
    :src: src/core/frame/py_frame.cc Frame::m__len__

    Returns the number of columns in the Frame, same as :attr:`.ncols`
    property.

    This special method is used by the python built-in function
    ``len()``, and allows the :class:`dt.Frame` class to satisfy python
    ``Iterable`` interface.
