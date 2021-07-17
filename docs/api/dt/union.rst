
.. xfunction:: datatable.union
    :src: src/core/set_funcs.cc union_
    :cvar: doc_dt_union
    :tests: tests/test-sets.py
    :signature: union(*frames)

    Find the union of values in all `frames`.

    Each frame should have only a single column or be empty. The values in each
    frame will be treated as a set, and this function will perform the
    `union operation <https://en.wikipedia.org/wiki/Union_(set_theory)>`_
    on these sets.

    The `dt.union(*frames)` operation is equivalent to
    `dt.unique(dt.rbind(*frames))`.

    Parameters
    ----------
    *frames: Frame, Frame, ...
        Input single-column frames.

    return: Frame
        A single-column frame. The column stype is the smallest common
        stype of columns in the `frames`.

    except: ValueError | NotImplementedError
        .. list-table::
            :widths: auto
            :class: api-table

            * - :exc:`dt.exceptions.ValueError`
              - raised when one of the input frames has more than one column.

            * - :exc:`dt.exceptions.NotImplementedError`
              - raised when one of the columns has stype `obj64`.


    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt
        >>>
        >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
        ...                'B': [None, 2, 3,4, 5],
        ...                'C': [1, 2, 1, 1, 2]})
        >>> df
           |     A      B      C
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     1     NA      1
         1 |     1      2      2
         2 |     2      3      1
         3 |     1      4      1
         4 |     2      5      2
        [5 rows x 3 columns]

    Union of all the columns in a frame::

        >>> dt.union(*df)
           |     A
           | int32
        -- + -----
         0 |    NA
         1 |     1
         2 |     2
         3 |     3
         4 |     4
         5 |     5
        [6 rows x 1 column]

    Union of two frames::

        >>> dt.union(df["A"], df["C"])
           |     A
           | int32
        -- + -----
         0 |     1
         1 |     2
        [2 rows x 1 column]


    See Also
    --------
    - :func:`intersect()` -- calculate the set intersection of values in the frames.
    - :func:`setdiff()` -- calculate the set difference between the frames.
    - :func:`symdiff()` -- calculate the symmetric difference between the sets of values in the frames.
    - :func:`unique()` -- find unique values in a frame.
