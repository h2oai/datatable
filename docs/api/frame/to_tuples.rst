
.. xmethod:: datatable.Frame.to_tuples
    :src: src/core/frame/to_python.cc Frame::to_tuples
    :cvar: doc_Frame_to_tuples
    :signature: to_tuples(self)

    Convert the frame into a list of tuples, by rows.

    Parameters
    ----------
    return: List[Tuple]
      Returns a list having :attr:`.nrows` tuples, where
      each tuple has length :attr:`.ncols` and contains data
      from each respective row of the frame.

    Examples
    --------
    >>> DT = dt.Frame(A=[1, 2, 3], B=["aye", "nay", "tain"])
    >>> DT.to_tuples()
    [(1, "aye"), (2, "nay"), (3, "tain")]
