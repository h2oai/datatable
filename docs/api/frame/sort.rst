
.. xmethod:: datatable.Frame.sort
    :src: src/core/sort.cc py::Frame::sort
    :cvar: doc_Frame_sort
    :signature: sort(self, *cols)

    Sort frame by the specified column(s).


    Parameters
    ----------
    cols: List[str | int]
        Names or indices of the columns to sort by. If no columns are
        given, the Frame will be sorted on all columns.

    return: Frame
        New Frame sorted by the provided column(s). The current frame
        remains unmodified.
