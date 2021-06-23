
.. xmethod:: datatable.Frame.__setitem__
    :src: src/core/frame/__getitem__.cc Frame::_main_getset
    :settable: R
    :signature: __setitem__(i, j[, by][, sort][, join])

    This methods updates values within the frame, or adds new columns to the
    frame.

    All parameters have the same meaning as in the getter
    :meth:`DT[i, j, ...] <datatable.Frame.__getitem__>`, with the only
    restriction that `j` must select to columns by reference (i.e. there
    could not be any computed columns there). On the other hand, `j` may
    contain columns that do not exist in the frame yet: these columns will be
    created.

    Parameters
    ----------
    i : ...
        Row selector.

    j : ...
        Column selector. Computed columns are forbidden, but not-existing (new)
        columns are allowed.

    by : by
        Groupby condition.

    join : join
        Join criterion.

    R : FExpr | List[FExpr] | Frame | type | None | bool | int | float | str
        The replacement for the selection on the left-hand-side.

        ``None`` | ``bool`` | ``int`` | ``float`` | ``str``
            A simple python scalar can be assigned to any-shape selection on
            the LHS. If `i` selects all rows (i.e. the assignment is of the
            form ``DT[:, j] = R``), then each column in `j` will be replaced
            with a constant column containing the value `R`.

            If, on the other hand, `i` selects only some rows, then the type of
            `R` must be consistent with the type of column(s) selected in `j`.
            In this case only cells in subset ``[i, j]`` will be updated with
            the value of ``R``; the columns may be promoted within their ltype
            if the value of ``R`` is large in magnitude.

        ``type`` | ``stype`` | ``ltype``
            Assigning a type to one or more columns will change the types of
            those columns. The row selector `i` must be "slice-all" ``:``.

        ``Frame`` | ``FExpr`` | ``List[FExpr]``
            When a frame or an expression is assigned, then the shape of the
            RHS must match the shape of the LHS. Similarly to the assignment
            of scalars, types must be compatible when assigning to a subset
            of rows.


    See Also
    --------
    - :func:`dt.update()` -- An alternative way to update values in the frame within
      ``DT[i, j]`` getter.

    - :meth:`.replace()` -- Search and replace for certain values within
      the entire frame.



.. xmethod:: datatable.Frame.__setitem__
    :src: src/core/frame/__getitem__.cc Frame::_main_getset
    :settable: R
    :noindex:

    __setitem__(j)
    --

    A simplified form of the setter, suitable for a single-column replacement.
    In this case `j` may only be an integer or a string.
