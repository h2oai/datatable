
.. xfunction:: datatable.str.split_into_nhot
    :src: src/core/str/split_into_nhot.cc py_split_into_nhot
    :tests: tests/munging/test-str.py
    :cvar: doc_str_split_into_nhot
    :signature: split_into_nhot(frame, sep=",", sort=False)

    Split and nhot-encode a single-column frame.

    Each value in the frame, having a single string column, is split according
    to the provided separator `sep`, the whitespace is trimmed, and
    the resulting pieces (labels) are converted into the individual columns
    of the output frame.


    Parameters
    ----------
    frame: Frame
        An input single-column frame. The column stype must be either `str32`
        or `str64`.

    sep: str
        Single-character separator to be used for splitting.

    sort: bool
        An option to control whether the resulting column names, i.e. labels,
        should be sorted. If set to `True`, the column names are returned in
        alphabetical order, otherwise their order is not guaranteed
        due to the algorithm parallelization.

    return: Frame
        The output frame. It will have as many rows as the input frame,
        and as many boolean columns as there were unique labels found.
        The labels will also become the output column names.

    except: ValueError | TypeError
        :exc:`dt.exceptions.ValueError`
            Raised if the input frame is missing or it has more
            than one column. It is also raised if `sep` is not a single-character
            string.

        :exc:`dt.exceptions.TypeError`
            Raised if the single column of `frame` has non-string stype.


    Examples
    --------

    .. code-block:: python

        >>> DT = dt.Frame(["cat,dog", "mouse", "cat,mouse", "dog,rooster", "mouse,dog,cat"])
        >>> DT
           | C0
           | str32
        -- + -------------
         0 | cat,dog
         1 | mouse
         2 | cat,mouse
         3 | dog,rooster
         4 | mouse,dog,cat
        [5 rows x 1 column]

        >>> dt.split_into_nhot(DT)
           |   cat    dog  mouse  rooster
           | bool8  bool8  bool8    bool8
        -- + -----  -----  -----  -------
         0 |     1      1      0        0
         1 |     0      0      1        0
         2 |     1      0      1        0
         3 |     0      1      0        1
         4 |     1      1      1        0
        [5 rows x 4 columns]
