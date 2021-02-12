
.. xmethod:: datatable.Namespace.__getitem__
    :src: src/core/expr/namespace.cc Namespace::m__getitem__

    __getitem__(self, *items)
    --

    Retrieve column(s) by their indices/names/types.

    By "retrieve" we actually mean that an expression is created
    such that when that expression is used within the
    :meth:`DT[i,j] <dt.Frame.__getitem__>` call, it would locate and
    return the specified column(s).


    Parameters
    ----------
    items: int | str | slice | None | type | stype | ltype | list | tuple
        The column selector:

        ``int``
            Retrieve the column at the specified index. For example,
            ``f[0]`` denotes the first column, while ``f[-1]`` is the
            last.

        ``str``
            Retrieve a column by name.

        ``slice``
            Retrieve a slice of columns from the namespace. Both integer
            and string slices are supported.

            Note that for string slicing, both the *start* and *stop* column
            names are included, unlike integer slicing, where the *stop* value
            is not included. Have a look at the examples below for more clarity.

        ``None``
            Retrieve no columns (an empty columnset).

        ``type`` | ``stype`` | ``ltype``
            Retrieve columns matching the specified type.

        ``list/tuple``
            Retrieve columns matching the column names/column positions/column types
            within the list/tuple.

            For example, ``f[0, -1]`` will return the first and last columns.
            Have a look at the examples below for more clarity.

    return: FExpr
        An expression that selects the specified column from a frame.


    See also
    --------
    - :ref:`f-expressions` -- user guide on using f-expressions.

    Notes
    -----
    .. x-version-changed:: 1.0.0

        :ref:`f-expressions` containing a list/tuple of
        column names/column positions/column types are
        accepted within the `j` selector.

    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f, by
        >>>
        >>> df = dt.Frame({'A': [1, 2, 3, 4],
        ...                'B': ["tolu", "sammy", "ogor", "boondocks"],
        ...                'C': [9.0, 10.0, 11.0, 12.0]})
        >>>
        >>> df
           |     A  B                C
           | int32  str32      float64
        -- + -----  ---------  -------
         0 |     1  tolu             9
         1 |     2  sammy           10
         2 |     3  ogor            11
         3 |     4  boondocks       12
        [4 rows x 3 columns]


    Select by column position::

        >>> df[:, f[0]]
           |     A
           | int32
        -- + -----
         0 |     1
         1 |     2
         2 |     3
         3 |     4
        [4 rows x 1 column]


    Select by column name::

        >>> df[:, f["A"]]
           |     A
           | int32
        -- + -----
         0 |     1
         1 |     2
         2 |     3
         3 |     4
        [4 rows x 1 column]


    Select a slice::

        >>> df[:, f[0 : 2]]
           |     A  B
           | int32  str32
        -- + -----  ---------
         0 |     1  tolu
         1 |     2  sammy
         2 |     3  ogor
         3 |     4  boondocks
        [4 rows x 2 columns]


    Slicing with column names::

        >>> df[:, f["A" : "C"]]
           |     A  B                C
           | int32  str32      float64
        -- + -----  ---------  -------
         0 |     1  tolu             9
         1 |     2  sammy           10
         2 |     3  ogor            11
         3 |     4  boondocks       12
        [4 rows x 3 columns]

    .. note:: For string slicing, **both** the start and stop are included; for integer slicing the stop is **not** included.

    Select by data type::

        >>> df[:, f[dt.str32]]
           | B
           | str32
        -- + ---------
         0 | tolu
         1 | sammy
         2 | ogor
         3 | boondocks
        [4 rows x 1 column]

        >>> df[:, f[float]]
           |       C
           | float64
        -- + -------
         0 |       9
         1 |      10
         2 |      11
         3 |      12
        [4 rows x 1 column]

    Select a list/tuple of columns by position::

        >>> df[:, f[0, 1]]

           |     A  B
           | int32  str32
        -- + -----  ---------
         0 |     1  tolu
         1 |     2  sammy
         2 |     3  ogor
         3 |     4  boondocks
        [4 rows x 2 columns]

    Or by column names::

        >>> df[:, f[("A", "B")]]
           |     A  B
           | int32  str32
        -- + -----  ---------
         0 |     1  tolu
         1 |     2  sammy
         2 |     3  ogor
         3 |     4  boondocks
        [4 rows x 2 columns]


    Note that in the code above, the parentheses are unnecessary, since tuples in python are defined
    by the presence of a comma. So the below code works as well::

        >>> df[:, f["A", "B"]]
           |     A  B
           | int32  str32
        -- + -----  ---------
         0 |     1  tolu
         1 |     2  sammy
         2 |     3  ogor
         3 |     4  boondocks
        [4 rows x 2 columns]


    Select a list/tuple of data types::

        >>> df[:, f[int, float]]
           |     A        C
           | int32  float64
        -- + -----  -------
         0 |     1        9
         1 |     2       10
         2 |     3       11
         3 |     4       12
        [4 rows x 2 columns]

    Passing ``None`` within an :ref:`f-expressions` returns an empty columnset::

        >>> df[:, f[None]]
           |
           |
        -- +
         0 |
         1 |
         2 |
         3 |
        [4 rows x 0 columns]







