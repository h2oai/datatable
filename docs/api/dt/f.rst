
.. xdata:: datatable.f
    :src: src/datatable/expr/expr.py f

    The main :class:`Namespace` object.

    The function of this object is that during the evaluation of a
    :meth:`DT[i,j] <dt.Frame.__getitem__>` call, the variable ``f``
    represents the columns of frame ``DT``.

    Specifically, *within* expression ``DT[i, j]`` the following
    is true:

    - ``f.A`` means "column A" of frame ``DT``;
    - ``f[2]`` means "3rd colum" of frame ``DT``;
    - ``f[int]`` means "all integer columns" of ``DT``;
    - ``f[:]`` means "all columns" of ``DT`` not including :func:`by` columns, however.


    See also
    --------
    - :data:`g` -- namespace for joined frames.
