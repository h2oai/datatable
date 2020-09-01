
.. xmethod:: datatable.Frame.__delitem__
    :src: src/core/frame/__getitem__.cc Frame::_main_getset

    __delitem__(i, j[, by][, sort][, join])
    --

    This methods deletes rows and columns that would have been selected from
    the frame if not for the ``del`` keyword.

    All parameters have the same meaning as in the getter
    :meth:`DT[i, j, ...] <datatable.Frame.__getitem__>`, with the only
    restriction that `j` must select columns from the main frame only (i.e. not
    from the joined frame(s)), and it must select them by reference. Selecting
    by reference means it should be possible to tell where each column was in
    the original frame.

    There are several modes of delete operation, depending on whether `i` or
    `j` are "slice-all" symbols:

    - `del DT[:, :]` removes everything from the frame, making it 0x0;
    - `del DT[:, j]` removes columns `j` from the frame;
    - `del DT[i, :]` removes rows `i` from the frame;
    - `del DT[i, j]` the shape of the frame remains the same, but the elements
      at ``[i, j]`` locations are replaced with NAs.
