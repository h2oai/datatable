
.. xmethod:: datatable.Frame.__reversed__
    :src: src/core/frame/__iter__.cc Frame::m__reversed__

    Returns an iterator over the frame's columns in reverse order.

    This is similar to :meth:`.__iter__()`, except that the columns
    are returned in the reverse order, i.e. ``frame[-1]``, ``frame[-2]``,
    ``frame[-3]``, etc.

    This function is not intended for manual use. Instead, it is
    invoked by Python builtin function :ext-func:`reversed() <reversed>`.

