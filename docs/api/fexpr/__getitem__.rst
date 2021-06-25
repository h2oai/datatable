
.. xmethod:: datatable.FExpr.__getitem__
    :src: src/core/expr/fexpr_slice.cc FExpr_Slice::evaluate_n
    :signature: __getitem__(self, selector)

    Apply a slice to the string column represented by this FExpr.


    Parameters
    ----------
    self: FExpr[str]

    selector: slice
        The slice will be applied to each value in the string column `self`.

    return: FExpr[str]


    Examples
    --------
    >>> DT = dt.Frame(season=["Winter", "Summer", "Autumn", "Spring"], i=[1, 2, 3, 4])
    >>> DT[:, {"start": f.season[:-f.i], "end": f.season[-f.i:]}]
       | start  end  
       | str32  str32
    -- + -----  -----
     0 | Winte  r    
     1 | Summ   er   
     2 | Aut    umn  
     3 | Sp     ring 
    [4 rows x 2 columns]


    See Also
    --------
    - :func:`dt.str.slice()` -- the equivalent function in :mod:`dt.str` module.
