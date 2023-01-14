
.. xfunction:: datatable.cbind
    :src: src/core/frame/cbind.cc py_cbind
    :cvar: doc_dt_cbind
    :signature: cbind(*frames, force=False)

    Create a new Frame by appending columns from several `frames`.

    .. note::
        In contrast to :meth:`dt.Frame.cbind()` that modifies
        the original Frame in-place and returns `None`,
        this function returns a new Frame and do not modify any
        of the `frames`.

        Therefore,::

            >>> DT = dt.cbind(*frames, force=force)

        is exactly equivalent to::

            >>> DT = dt.Frame()
            >>> DT.cbind(*frames, force=force)

    Parameters
    ----------
    frames: Frame | List[Frame] | None
        The list/tuple/sequence/generator expression of Frames to append.
        It may also contain `None` values, which will be simply
        skipped.

    force: bool
        If `True`, allows Frames to be appended even if they have unequal
        number of rows. The resulting Frame will have number of rows equal
        to the largest among all Frames. Those Frames which have less
        than the largest number of rows, will be padded with NAs (with the
        exception of Frames having just 1 row, which will be replicated
        instead of filling with NAs).

    return: Frame
        A new frame that is created by appending columns from `frames`.


    See also
    --------
    - :func:`rbind()` -- function for row-binding several frames.
    - :meth:`dt.Frame.cbind()` -- Frame method for cbinding several frames to
      another.


    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f
        >>>
        >>> DT = dt.Frame(A=[1, 2, 3], B=[4, 7, 0])
        >>> DT
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1      4
         1 |     2      7
         2 |     3      0
        [3 rows x 2 columns]

        >>> frame1 = dt.Frame(N=[-1, -2, -5])
        >>> frame1
           |     N
           | int32
        -- + -----
         0 |    -1
         1 |    -2
         2 |    -5
        [3 rows x 1 column]

        >>> dt.cbind([DT, frame1])
           |     A      B      N
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     1      4     -1
         1 |     2      7     -2
         2 |     3      0     -5
        [3 rows x 3 columns]

    If the number of rows are not equal, you can force the binding by setting
    the `force` parameter to `True`::

        >>> frame2 = dt.Frame(N=[-1, -2, -5, -20])
        >>> frame2
           |     N
           | int32
        -- + -----
         0 |    -1
         1 |    -2
         2 |    -5
         3 |   -20
        [4 rows x 1 column]

        >>> dt.cbind([DT, frame2], force=True)
           |     A      B      N
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     1      4     -1
         1 |     2      7     -2
         2 |     3      0     -5
         3 |    NA     NA    -20
        [4 rows x 3 columns]
