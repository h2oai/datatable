
.. xmethod:: datatable.FExpr.alias
    :src: src/core/expr/fexpr.cc PyFExpr::alias
    :cvar: doc_FExpr_alias
    :signature: alias(self, *names)

    Assign new names to the columns from the current ``FExpr``.


    Parameters
    ----------
    names: str | List[str] | Tuple[str]
        New names that should be assigned to the columns from
        the current ``FExpr``.

    return: FExpr
        New ``FExpr`` which sets new `names` on the current one.


    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f, by
        >>>
        >>> DT = dt.Frame([[1, 2, 3], ["one", "two", "three"]])
        >>> DT
           |    C0  C1
           | int32  str32
        -- + -----  -----
         0 |     1  a
         1 |     2  b
         2 |     3  c
        [3 rows x 2 columns]


    Assign new names when selecting data from the frame::

        >>> DT[:, f[:].alias("numbers", "strings")]
           | numbers  strings
           |   int32  str32
        -- + -------  -------
         0 |       1  one
         1 |       2  two
         2 |       3  three
        [3 rows x 2 columns]


    Assign new name for the newly computed column only::

        >>> DT[:, [f[:], (f[0] * f[0]).alias("numbers_squared")]]
           |    C0  C1     numbers_squared
           | int32  str32            int32
        -- + -----  -----  ---------------
         0 |     1  one                  1
         1 |     2  two                  4
         2 |     3  three                9
        [3 rows x 3 columns]


    Assign new name for the group by column::

        >>> DT[:, f[1], by(f[0].alias("numbers"))]
           | numbers  C1
           |   int32  str32
        -- + -------  -----
         0 |       1  one
         1 |       2  two
         2 |       3  three
        [3 rows x 2 columns]
