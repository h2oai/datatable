
.. xmethod:: datatable.Frame.to_list
    :src: src/core/frame/to_python.cc Frame::to_list
    :cvar: doc_Frame_to_list
    :signature: to_list(self)

    Convert the frame into a list of lists, by columns.


    Parameters
    ----------
    return: List[List]
        A list of :attr:`.ncols` lists, each inner list
        representing one column of the frame.


    Examples
    --------
    >>> DT = dt.Frame(A=[1, 2, 3], B=["aye", "nay", "tain"])
    >>> DT.to_list()
    [[1, 2, 3], ["aye", "nay", "tain"]]

    >>> dt.Frame(id=range(10)).to_list()
    [[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]]
