
.. xmethod:: datatable.Frame.replace
    :src: src/core/frame/replace.cc Frame::replace
    :cvar: doc_Frame_replace
    :signature: replace(self, replace_what, replace_with)

    Replace given value(s) `replace_what` with `replace_with` in the entire Frame.

    For each replace value, this method operates only on columns of types
    appropriate for that value. For example, if `replace_what` is a list
    `[-1, math.inf, None, "??"]`, then the value `-1` will be replaced in integer
    columns only, `math.inf` only in real columns, `None` in columns of all types,
    and finally `"??"` only in string columns.

    The replacement value must match the type of the target being replaced,
    otherwise an exception will be thrown. That is, a bool must be replaced with a
    bool, an int with an int, a float with a float, and a string with a string.
    The `None` value (representing NA) matches any column type, and therefore can
    be used as either replacement target, or replace value for any column. In
    particular, the following is valid: `DT.replace(None, [-1, -1.0, ""])`. This
    will replace NA values in int columns with `-1`, in real columns with `-1.0`,
    and in string columns with an empty string.

    The replace operation never causes a column to change its logical type. Thus,
    an integer column will remain integer, string column remain string, etc.
    However, replacing may cause a column to change its stype, provided that
    ltype remains constant. For example, replacing `0` with `-999` within an `int8`
    column will cause that column to be converted into the `int32` stype.

    Parameters
    ----------
    replace_what: None, bool, int, float, list, or dict
        Value(s) to search for and replace.

    replace_with: single value, or list
        The replacement value(s). If `replace_what` is a single value, then this
        must be a single value too. If `replace_what` is a list, then this could
        be either a single value, or a list of the same length. If `replace_what`
        is a dict, then this value should not be passed.

    (return): None
        Nothing is returned, the replacement is performed in-place.


    Examples
    --------
    >>> df = dt.Frame([1, 2, 3] * 3)
    >>> df.replace(1, -1)
    >>> df
       |    C0
       | int32
    -- + -----
     0 |    -1
     1 |     2
     2 |     3
     3 |    -1
     4 |     2
     5 |     3
     6 |    -1
     7 |     2
     8 |     3
    [9 rows x 1 column]

    >>> df.replace({-1: 100, 2: 200, "foo": None})
    >>> df
       |    C0
       | int32
    -- + -----
     0 |   100
     1 |   200
     2 |     3
     3 |   100
     4 |   200
     5 |     3
     6 |   100
     7 |   200
     8 |     3
    [9 rows x 1 column]
