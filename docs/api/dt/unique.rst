
.. xfunction:: datatable.unique
    :src: src/core/set_funcs.cc unique
    :cvar: doc_dt_unique
    :tests: tests/test-sets.py
    :signature: unique(frame)

    Find the unique values in all the columns of the `frame`.

    This function sorts the values in order to find the uniques, so the return
    values will be ordered. However, this should be considered an implementation
    detail: in the future datatable may switch to a different algorithm,
    such as hash-based, which may return the results in a different order.


    Parameters
    ----------
    frame: Frame
        Input frame.

    return: Frame
        A single-column frame consisting of unique values found in `frame`.
        The column stype is the smallest common stype for all the
        `frame` columns.

    except: NotImplementedError
        The exception is raised when one of the frame columns has stype `obj64`.


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

    Unique values in the entire frame::

        >>> dt.unique(df)
           |    C0
           | int32
        -- + -----
         0 |    NA
         1 |     1
         2 |     2
         3 |     3
         4 |     4
         5 |     5
        [6 rows x 1 column]

    Unique values in a frame with a single column::

        >>> dt.unique(df["A"])
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
    - :func:`union()` -- calculate the union of values in the frames.
