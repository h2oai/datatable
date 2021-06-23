
.. xmethod:: datatable.Frame.tail
    :src: src/core/frame/py_frame.cc Frame::tail
    :cvar: doc_Frame_tail
    :signature: tail(self, n=10)

    Return the last `n` rows of the frame.

    If the number of rows in the frame is less than `n`, then all rows
    are returned.

    This is a convenience function and it is equivalent to `DT[-n:, :]`
    (except when `n` is 0).

    Parameters
    ----------
    n : int
        The maximum number of rows to return, 10 by default. This number
        cannot be negative.

    return: Frame
        A frame containing the last up to `n` rows from the original
        frame, and same columns.


    Examples
    --------

    >>> DT = dt.Frame(A=["apples", "bananas", "cherries", "dates",
    ...                  "eggplants", "figs", "grapes", "kiwi"])
    >>> DT.tail(3)
       | A
       | str32
    -- + ------
     0 | figs
     1 | grapes
     2 | kiwi
    [3 rows x 1 column]


    See also
    --------
    - :meth:`.head` -- return the first `n` rows of the Frame.
