
.. xfunction:: datatable.rbind
    :src: src/core/frame/rbind.cc py_rbind
    :tests: tests/munging/test-rbind.py
    :cvar: doc_dt_rbind
    :signature: rbind(*frames, force=False, bynames=True)

    Produce a new frame by appending rows of several `frames`.

    This function is equivalent to::

        >>> dt.Frame().rbind(*frames, force=force, by_names=by_names)


    Parameters
    ----------
    frames: Frame | List[Frame] | None

    force: bool

    bynames: bool

    return: Frame


    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt
        >>>
        >>> DT1 = dt.Frame({"Weight": [5, 4, 6], "Height": [170, 172, 180]})
        >>> DT1
           | Weight  Height
           |  int32   int32
        -- + ------  ------
         0 |      5     170
         1 |      4     172
         2 |      6     180
        [3 rows x 2 columns]

        >>> DT2 = dt.Frame({"Height": [180, 181, 169], "Weight": [4, 4, 5]})
        >>> DT2
           | Weight  Height
           |  int32   int32
        -- + ------  ------
         0 |      4     180
         1 |      4     181
         2 |      5     169
        [3 rows x 2 columns]

        >>> dt.rbind(DT1, DT2)
           | Weight  Height
           |  int32   int32
        -- + ------  ------
         0 |      5     170
         1 |      4     172
         2 |      6     180
         3 |      4     180
         4 |      4     181
         5 |      5     169
        [6 rows x 2 columns]

    :func:`rbind()` by default combines frames by names. The frames can also be
    bound by column position by setting the `bynames` parameter to ``False``::

        >>> dt.rbind(DT1, DT2, bynames = False)
           | Weight  Height
           |  int32   int32
        -- + ------  ------
         0 |      5     170
         1 |      4     172
         2 |      6     180
         3 |    180       4
         4 |    181       4
         5 |    169       5
        [6 rows x 2 columns]


    If the number of columns are not equal or the column names are different,
    you can force the row binding by setting the `force` parameter to `True`::

        >>> DT2["Age"] = dt.Frame([25, 50, 67])
        >>> DT2
           | Weight  Height    Age
           |  int32   int32  int32
        -- + ------  ------  -----
         0 |      4     180     25
         1 |      4     181     50
         2 |      5     169     67
        [3 rows x 3 columns]

        >>> dt.rbind(DT1, DT2, force = True)
           | Weight  Height    Age
           |  int32   int32  int32
        -- + ------  ------  -----
         0 |      5     170     NA
         1 |      4     172     NA
         2 |      6     180     NA
         3 |      4     180     25
         4 |      4     181     50
         5 |      5     169     67
        [6 rows x 3 columns]


    See also
    --------
    - :func:`cbind()` -- function for col-binding several frames.
    - :meth:`dt.Frame.rbind()` -- Frame method for rbinding some frames to
      another.
