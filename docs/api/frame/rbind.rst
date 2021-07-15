
.. xmethod:: datatable.Frame.rbind
    :src: src/core/frame/rbind.cc Frame::rbind
    :cvar: doc_Frame_rbind
    :signature: rbind(self, *frames, force=False, bynames=True)

    Append rows of `frames` to the current frame.

    This is equivalent to `list.extend()` in Python: the frames are
    combined by rows, i.e. rbinding a frame of shape [n x k] to a Frame
    of shape [m x k] produces a frame of shape [(m + n) x k].

    This method modifies the current frame in-place. If you do not want
    the current frame modified, then use the :func:`dt.rbind()` function.

    If frame(s) being appended have columns of types different from the
    current frame, then these columns will be promoted according to the
    standard promotion rules. In particular, booleans can be promoted into
    integers, which in turn get promoted into floats. However, they are
    not promoted into strings or objects.

    If frames have columns of incompatible types, a TypeError will be
    raised.

    If you need to append multiple frames, then it is more efficient to
    collect them into an array first and then do a single `rbind()`, than
    it is to append them one-by-one in a loop.

    Appending data to a frame opened from disk will force loading the
    current frame into memory, which may fail with an OutOfMemory
    exception if the frame is sufficiently big.

    Parameters
    ----------
    frames: Frame | List[Frame]
        One or more frames to append. These frames should have the same
        columnar structure as the current frame (unless option `force` is
        used).

    force: bool
        If True, then the frames are allowed to have mismatching set of
        columns. Any gaps in the data will be filled with NAs.

        In addition, when this parameter is True, rbind will no longer
        produce an error when combining columns of unrelated types.
        Instead, both columns will be converted into strings.

    bynames: bool
        If True (default), the columns in frames are matched by their
        names. For example, if one frame has columns ["colA", "colB",
        "colC"] and the other ["colB", "colA", "colC"] then we will swap
        the order of the first two columns of the appended frame before
        performing the append. However if `bynames` is False, then the
        column names will be ignored, and the columns will be matched
        according to their order, i.e. i-th column in the current frame
        to the i-th column in each appended frame.

    return: None
