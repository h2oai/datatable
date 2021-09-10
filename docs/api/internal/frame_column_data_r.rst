
.. xfunction:: datatable.internal.frame_column_data_r
    :src: src/core/datatablemodule.cc frame_column_data_r
    :cvar: doc_internal_frame_column_data_r
    :signature: frame_column_data_r(frame, i)

    Return C pointer to the main data array of the column `frame[i]`.
    The column will be materialized if it was virtual.


    Parameters
    ----------
    frame: Frame
        The :class:`dt.Frame` where to look up the column.

    i: int
        The index of a column, in the range ``[0; ncols)``.

    return: ctypes.c_void_p
        The pointer to the column's internal data.
