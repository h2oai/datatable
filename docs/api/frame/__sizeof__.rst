
.. xmethod:: datatable.Frame.__sizeof__
    :src: src/core/frame/__sizeof__.cc Frame::m__sizeof__
    :cvar: doc_Frame___sizeof__
    :signature: __sizeof__(self)

    Return the size of this Frame in memory.

    The function attempts to compute the total memory size of the frame
    as precisely as possible. In particular, it takes into account not
    only the size of data in columns, but also sizes of all auxiliary
    internal structures.

    Special cases: if frame is a view (say, `d2 = DT[:1000, :]`), then
    the reported size will not contain the size of the data, because that
    data "belongs" to the original datatable and is not copied. However if
    a frame selects only a subset of columns (say, `d3 = DT[:, :5]`),
    then a view is not created and instead the columns are copied by
    reference. Frame `d3` will report the "full" size of its columns,
    even though they do not occupy any extra memory compared to `DT`.
    This behavior may be changed in the future.

    This function is not intended for manual use. Instead, in order to
    get the size of a frame `DT`, call `sys.getsizeof(DT)`.
