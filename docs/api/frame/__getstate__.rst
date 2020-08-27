
.. xmethod:: datatable.Frame.__getstate__
    :src: src/core/frame/__init__.cc Frame::m__getstate__

    This method allows the frame to be ``pickle``-able.

    Pickling a Frame involves saving it into a ``bytes`` object in Jay format,
    but may be less efficient than saving into a file directly because Python
    creates a copy of the data for the bytes object.

    See :meth:`.to_jay()` for more details and caveats about saving into Jay
    format.
