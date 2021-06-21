
.. xmethod:: datatable.Frame.colindex
    :src: src/core/frame/names.cc Frame::colindex
    :tests: tests/frame/test-colindex.py
    :cvar: doc_Frame_colindex
    :signature: colindex(self, column)

    Return the position of the `column` in the Frame.

    The index of the first column is `0`, just as with regular python
    lists.


    Parameters
    ----------
    column: str | int | FExpr
        If string, then this is the name of the column whose index you
        want to find.

        If integer, then this represents a column's index. The return
        value is thus the same as the input argument `column`, provided
        that it is in the correct range. If the `column` argument is
        negative, then it is interpreted as counting from the end of the
        frame. In this case the positive value `column + ncols` is
        returned.

        Lastly, `column` argument may also be an
        :ref:`f-expression <f-expressions>` such as `f.A` or `f[3]`. This
        case is treated as if the argument was simply `"A"` or `3`. More
        complicated f-expressions are not allowed and will result in a
        `TypeError`.

    return: int
        The numeric index of the provided `column`. This will be an
        integer between `0` and `self.ncols - 1`.

    except: KeyError | IndexError
        .. list-table::
            :widths: auto
            :class: api-table

            * - :exc:`dt.exceptions.KeyError`
              - raised if the `column` argument is a string, and the column with
                such name does not exist in the frame. When this exception is
                thrown, the error message may contain suggestions for up to 3
                similarly looking column names that actually exist in the Frame.

            * - :exc:`dt.exceptions.IndexError`
              - raised if the `column` argument is an integer that is either greater
                than or equal to :attr:`.ncols` or less than `-ncols`.

    Examples
    --------

    >>> df = dt.Frame(A=[3, 14, 15], B=["enas", "duo", "treis"],
    ...               C=[0, 0, 0])
    >>> df.colindex("B")
    1
    >>> df.colindex(-1)
    2

    >>> from datatable import f
    >>> df.colindex(f.A)
    0
