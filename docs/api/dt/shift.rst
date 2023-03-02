
.. xfunction:: datatable.shift
    :src: src/core/expr/head_func_shift.cc pyfn_shift
    :cvar: doc_dt_shift
    :tests: tests/dt/test-shift.py
    :signature: shift(cols, n=1)

    Shift columns' data forward or backwards. This function is group-aware,
    i.e. in the presence of a groupby it will shift data separately
    within each group.


    Parameters
    ----------
    cols: FExpr
        Input data to be shifted.

    n: int
        The shift amount. For positive `n`, the data are shifted forward, i.e.
        a "lag" column is created. For negative `n`, the data are shifted backwards
        creating a "lead" column.

    return: FExpr
        f-expression that shifts input data by `n` rows. The resulting data
        will have the same number of rows as `cols`. Depending on `n`,
        i.e. positive/negative, `n` observations in the beginning/end
        will become missing and `n` observations at the end/beginning
        will be discarded.


    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f, by
        >>>
        >>> DT = dt.Frame({"object": [1, 1, 1, 2, 2],
        >>>                "period": [1, 2, 4, 4, 23],
        >>>                "value": [24, 67, 89, 5, 23]})
        >>>
        >>> DT
           | object  period  value
           |  int32   int32  int32
        -- + ------  ------  -----
         0 |      1       1     24
         1 |      1       2     67
         2 |      1       4     89
         3 |      2       4      5
         4 |      2      23     23
        [5 rows x 3 columns]

    Shift forward by creating a "lag" column::

        >>> DT[:, dt.shift(f.period,  n = 3)]
           | period
           |  int32
        -- + ------
         0 |     NA
         1 |     NA
         2 |     NA
         3 |      1
         4 |      2
        [5 rows x 1 column]

    Shift backwards by creating "lead" columns::

        >>> DT[:, dt.shift(f[:], n = -3)]
           | object  period  value
           |  int32   int32  int32
        -- + ------  ------  -----
         0 |      2       4      5
         1 |      2      23     23
         2 |     NA      NA     NA
         3 |     NA      NA     NA
         4 |     NA      NA     NA
        [5 rows x 3 columns]

    Shift within groups, i.e. in the presence of :func:`by()`::

        >>> DT[:, f[:].extend({"prev_value": dt.shift(f.value)}), by("object")]
           | object  period  value  prev_value
           |  int32   int32  int32       int32
        -- + ------  ------  -----  ----------
         0 |      1       1     24          NA
         1 |      1       2     67          24
         2 |      1       4     89          67
         3 |      2       4      5          NA
         4 |      2      23     23           5
        [5 rows x 4 columns]

