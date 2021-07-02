
.. xmethod:: datatable.Frame.copy
    :src: src/core/frame/py_frame.cc Frame::copy
    :tests: tests/frame/test-copy.py
    :cvar: doc_Frame_copy
    :signature: copy(self, deep=False)

    Make a copy of the frame.

    The returned frame will be an identical copy of the original,
    including column names, types, and keys.

    By default, copying is shallow with copy-on-write semantics. This
    means that only the minimal information about the frame is copied,
    while all the internal data buffers are shared between the copies.
    Nevertheless, due to the copy-on-write semantics, any changes made to
    one of the frames will not propagate to the other; instead, the data
    will be copied whenever the user attempts to modify it.

    It is also possible to explicitly request a deep copy of the frame
    by setting the parameter `deep` to `True`. With this flag, the
    returned copy will be truly independent from the original. The
    returned frame will also be fully materialized in this case.


    Parameters
    ----------
    deep: bool
        Flag indicating whether to return a "shallow" (default), or a
        "deep" copy of the original frame.

    return: Frame
        A new Frame, which is the copy of the current frame.


    Examples
    --------

    >>> DT1 = dt.Frame(range(5))
    >>> DT2 = DT1.copy()
    >>> DT2[0, 0] = -1
    >>> DT2
       |    C0
       | int32
    -- + -----
     0 |    -1
     1 |     1
     2 |     2
     3 |     3
     4 |     4
    [5 rows x 1 column]
    >>> DT1
       |    C0
       | int32
    -- + -----
     0 |     0
     1 |     1
     2 |     2
     3 |     3
     4 |     4
    [5 rows x 1 column]


    Notes
    -----
    - Non-deep frame copy is a very low-cost operation: its speed depends
      on the number of columns only, not on the number of rows. On a
      regular laptop copying a 100-column frame takes about 30-50µs.

    - Deep copying is more expensive, since the data has to be physically
      written to new memory, and if the source columns are virtual, then
      they need to be computed too.

    - Another way to create a copy of the frame is using a `DT[i, j]`
      expression (however, this will not copy the key property)::

        >>> DT[:, :]

    - `Frame` class also supports copying via the standard Python library
      ``copy``::

        >>> import copy
        >>> DT_shallow_copy = copy.copy(DT)
        >>> DT_deep_copy = copy.deepcopy(DT)
