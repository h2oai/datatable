
.. xfunction:: datatable.setdiff
    :src: src/core/set_funcs.cc setdiff
    :cvar: doc_dt_setdiff
    :tests: tests/test-sets.py
    :signature: setdiff(frame0, *frames)

    Find the set difference between `frame0` and the other `frames`.

    Each frame should have only a single column or be empty.
    The values in each frame will be treated as a set, and this function will
    compute the
    `set difference  <https://en.wikipedia.org/wiki/Complement_(set_theory)#Relative_complement>`_
    between the `frame0` and the union of the other
    frames, returning those values that are present in the `frame0`,
    but not present in any of the `frames`.

    Parameters
    ----------
    frame0: Frame
        Input single-column frame.

    *frames: Frame, Frame, ...
        Input single-column frames.

    return: Frame
        A single-column frame. The column stype is the smallest common
        stype of columns from the `frames`.

    except: ValueError | NotImplementedError
        .. list-table::
            :widths: auto
            :class: api-table

            * - :exc:`dt.exceptions.ValueError`
              - raised when one of the input frames, i.e. `frame0`
                or any one from the `frames`, has more than one column.

            * - :exc:`dt.exceptions.NotImplementedError`
              - raised when one of the columns has stype `obj64`.

    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt
        >>>
        >>> s1 = dt.Frame([4, 5, 6, 20, 42])
        >>> s2 = dt.Frame([1, 2, 3, 5, 42])
        >>>
        >>> s1
           |    C0
           | int32
        -- + -----
         0 |     4
         1 |     5
         2 |     6
         3 |    20
         4 |    42
        [5 rows x 1 column]

        >>> s2
           |    C0
           | int32
        -- + -----
         0 |     1
         1 |     2
         2 |     3
         3 |     5
         4 |    42
        [5 rows x 1 column]

    Set difference of the two frames::

        >>> dt.setdiff(s1, s2)
           |    C0
           | int32
        -- + -----
         0 |     4
         1 |     6
         2 |    20
        [3 rows x 1 column]



    See Also
    --------
    - :func:`intersect()` -- calculate the set intersection of values in the frames.
    - :func:`symdiff()` -- calculate the symmetric difference between the sets of values in the frames.
    - :func:`union()` -- calculate the union of values in the frames.
    - :func:`unique()` -- find unique values in a frame.
