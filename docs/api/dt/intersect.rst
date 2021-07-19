
.. xfunction:: datatable.intersect
    :src: src/core/set_funcs.cc intersect
    :cvar: doc_dt_intersect
    :tests: tests/test-sets.py
    :signature: intersect(*frames)

    Find the intersection of sets of values in the `frames`.

    Each frame should have only a single column or be empty.
    The values in each frame will be treated as a set, and this function will
    perform the
    `intersection operation <https://en.wikipedia.org/wiki/Intersection_(set_theory)>`_
    on these sets, returning those values that are present in each
    of the provided `frames`.

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


    Intersection of the two frames::

        >>> dt.intersect([s1, s2])
           |    C0
           | int32
        -- + -----
         0 |     5
         1 |    42
        [2 rows x 1 column]



    See Also
    --------
    - :func:`setdiff()` -- calculate the set difference between the frames.
    - :func:`symdiff()` -- calculate the symmetric difference between the sets of values in the frames.
    - :func:`union()` -- calculate the union of values in the frames.
    - :func:`unique()` -- find unique values in a frame.
