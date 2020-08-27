.. py:currentmodule:: datatable

.. xmethod:: datatable.Frame.__repr__
    :src: src/core/frame/__repr__.cc Frame::m__repr__

    Returns a simple representation of the frame as a string. This
    method is used by Python's built-in function ``repr()``.

    The returned string has the following format::

    	"<Frame#{ID} {nrows}x{ncols}>"

    where ``{ID}`` is the value of ``id(frame)`` in hex format. Thus,
    each frame has its own unique id, though after one frame is
    deleted it's id may be reused for another frame.
