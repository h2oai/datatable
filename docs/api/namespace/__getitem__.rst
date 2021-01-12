
.. xmethod:: datatable.Namespace.__getitem__
    :src: src/core/expr/namespace.cc Namespace::m__getitem__

    __getitem__(self, item)
    --

    Retrieve column(s) by their indices/names/types.

    Also retrieve column(s) via :ref:`f-expressions`
    containing a list/tuple of
    column names/column positions/column types.

    By "retrieve" we actually mean that an expression is created
    such that when that expression is used within the
    :meth:`DT[i,j] <dt.Frame.__getitem__>` call, it would locate and
    return the specified column(s).


    Parameters
    ----------
    item: int | str | slice | None | type | stype | ltype | Expr
        The column selector:

        ``int``
            Retrieve the column at the specified index. For example,
            ``f[0]`` denotes the first column, whlie ``f[-1]`` is the
            last.

        ``str``
            Retrieve a column by name.

        ``slice``
            Retrieve a slice of columns from the namespace. Both integer
            and string slices are supported.

        ``None``
            Retrieve no columns (an empty columnset).

        ``type`` | ``stype`` | ``ltype``
            Retrieve columns matching the specified type.

        ``Expr``
            Retrieve columns matching the column names/column positions/column types
            within the list/tuple.  For example, ``f[0, -1]`` will return the first and
            last columns.

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
