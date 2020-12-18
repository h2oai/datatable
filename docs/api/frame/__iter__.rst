
.. xmethod:: datatable.Frame.__iter__
    :src: src/core/frame/__iter__.cc Frame::m__iter__

    Returns an iterator over the frame's columns.

    The iterator is a light-weight object of type ``frame_iterator``,
    which yields consequent columns of the frame with each iteration.

    Thus, the iterator produces the sequence ``frame[0], frame[1],
    frame[2], ...`` until the end of the frame. This works even if
    the user adds or deletes columns in the frame while iterating.
    Be careful when inserting/deleting columns at an index that was
    already iterated over, as it will cause some columns to be
    skipped or visited more than once.

    This method is not intended for manual use. Instead, it is
    invoked by Python runtime either when you call :ext-func:`iter() <iter>`,
    or when you use the frame in a loop::

        >>> for column in frame:
        ...     # column is a Frame of shape (frame.nrows, 1)
        ...     ...


    See Also
    --------
    - :meth:`.__reversed__()`
