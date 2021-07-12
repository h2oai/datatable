
.. xmethod:: datatable.Frame.to_dict
    :src: src/core/frame/to_python.cc Frame::to_dict
    :cvar: doc_Frame_to_dict
    :signature: to_dict(self)

    Convert the frame into a dictionary of lists, by columns.

    In Python 3.6+ the order of records in the dictionary will be the
    same as the order of columns in the frame.

    Parameters
    ----------
    return: Dict[str, List]
        Dictionary with :attr:`.ncols` records. Each record
        represents a single column: the key is the column's name, and the
        value is the list with the column's data.

    Examples
    --------
    >>> DT = dt.Frame(A=[1, 2, 3], B=["aye", "nay", "tain"])
    >>> DT.to_dict()
    {"A": [1, 2, 3], "B": ["aye", "nay", "tain"]}

    See also
    --------
    - :meth:`.to_list`: convert the frame into a list of lists
    - :meth:`.to_tuples`: convert the frame into a list of tuples by rows
