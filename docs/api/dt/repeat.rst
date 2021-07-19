
.. xfunction:: datatable.repeat
    :src: src/core/frame/repeat.cc repeat
    :cvar: doc_dt_repeat
    :signature: repeat(frame, n)

    Concatenate ``n`` copies of the ``frame`` by rows and return the result.

    This is equivalent to ``dt.rbind([frame] * n)``.


    Example
    -------
    ::

        >>> from datatable import dt
        >>> DT = dt.Frame({"A": [1, 1, 2, 1, 2],
        ...                "B": [None, 2, 3, 4, 5]})
        >>> DT
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1     NA
         1 |     1      2
         2 |     2      3
         3 |     1      4
         4 |     2      5
        [5 rows x 2 columns]

    ::

        >>> dt.repeat(DT, 2)
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1     NA
         1 |     1      2
         2 |     2      3
         3 |     1      4
         4 |     2      5
         5 |     1     NA
         6 |     1      2
         7 |     2      3
         8 |     1      4
         9 |     2      5
        [10 rows x 2 columns]
