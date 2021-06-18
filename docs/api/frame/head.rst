
.. xmethod:: datatable.Frame.head
    :src: src/core/frame/py_frame.cc Frame::head
    :cvar: doc_Frame_head
    :signature: head(self, n=10)

    Return the first `n` rows of the frame.

    If the number of rows in the frame is less than `n`, then all rows
    are returned.

    This is a convenience function and it is equivalent to `DT[:n, :]`.


    Parameters
    ----------
    n : int
        The maximum number of rows to return, 10 by default. This number
        cannot be negative.

    return: Frame
        A frame containing the first up to `n` rows from the original
        frame, and same columns.


    Examples
    --------
    >>> DT = dt.Frame(A=["apples", "bananas", "cherries", "dates",
    ...                  "eggplants", "figs", "grapes", "kiwi"])
    >>> DT.head(4)
       | A
       | str32
    -- + --------
     0 | apples
     1 | bananas
     2 | cherries
     3 | dates
    [4 rows x 1 column]


    See also
    --------
    - :meth:`.tail` -- return the last `n` rows of the Frame.

