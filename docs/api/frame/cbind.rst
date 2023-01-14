
.. xmethod:: datatable.Frame.cbind
    :src: src/core/frame/cbind.cc Frame::cbind
    :cvar: doc_Frame_cbind
    :signature: cbind(self, *frames, force=False)

    Append columns of one or more `frames` to the current Frame.

    For example, if the current frame has `n` columns, and you are
    appending another frame with `k` columns, then after this method
    succeeds, the current frame will have `n + k` columns. Thus, this
    method is roughly equivalent to `pandas.concat(axis=1)`.

    The frames being cbound must all either have the same number of rows,
    or some of them may have only a single row. Such single-row frames
    will be automatically expanded, replicating the value as needed.
    This makes it easy to create constant columns or to append reduction
    results (such as min/max/mean/etc) to the current Frame.

    If some of the `frames` have an incompatible number of rows, then the
    operation will fail with an :exc:`dt.exceptions.InvalidOperationError`.
    However, if you set the flag `force` to True, then the error will no
    longer be raised - instead all frames that are shorter than the others
    will be padded with NAs.

    If the frames being appended have the same column names as the current
    frame, then those names will be :ref:`mangled <name-mangling>`
    to ensure that the column names in the current frame remain unique.
    A warning will also be issued in this case.

    .. note::
        Since this method modifies the original Frame in-place, it will
        have no visible effect when called on a Frame, that is not
        associated with any variable.


    Parameters
    ----------
    frames: Frame | List[Frame] | None
        The list/tuple/sequence/generator expression of Frames to append
        to the current frame. It may also contain `None` values,
        which will be simply skipped.

    force: bool
        If `True`, allows Frames to be appended even if they have unequal
        number of rows. The resulting Frame will have number of rows equal
        to the largest among all Frames. Those Frames which have less
        than the largest number of rows, will be padded with NAs (with the
        exception of Frames having just 1 row, which will be replicated
        instead of filling with NAs).

    return: None
        This method alters the current frame in-place, and doesn't return
        anything.

    except: InvalidOperationError
        If trying to cbind frames with the number of rows different from
        the current frame's, and the option `force` is not set.


    Notes
    -----

    Cbinding frames is a very cheap operation: the columns are copied by
    reference, which means the complexity of the operation depends only
    on the number of columns, not on the number of rows. Still, if you
    are planning to cbind a large number of frames, it will be beneficial
    to collect them in a list first and then call a single `cbind()`
    instead of cbinding them one-by-one.

    It is also possible to cbind frames using the standard `DT[i, j]` syntax::

        >>> df[:, update(**frame1, **frame2, ...)]

    Or, if you need to append just a single column::

        >>> df["newcol"] = frame1


    See also
    --------
    - :func:`datatable.cbind` -- function for cbinding frames
      "out-of-place" instead of in-place;

    - :meth:`.rbind()` -- method for row-binding frames.


    Examples
    --------

    >>> DT0 = dt.Frame(A=[1, 2, 3], B=[4, 7, 0])
    >>> DT1 = dt.Frame(N=[-1, -2, -5])
    >>> DT0.cbind(DT1)
    >>> DT0
       |     A      B      N
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1      4     -1
     1 |     2      7     -2
     2 |     3      0     -5
    [3 rows x 3 columns]

    At the same time, doing

    >>> DT0[:, :].cbind(DT1)

    will have no effect on ``DT0``, because ``DT0[:, :]`` is a different view frame
    that is not assigned to any variable.

