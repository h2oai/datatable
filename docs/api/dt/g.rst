
.. xdata:: datatable.g
    :src: src/datatable/expr/expr.py g

    Secondary :class:`Namespace` object.

    The function of this object is that during the evaluation of a
    :meth:`DT[..., join(X)] <dt.Frame.__getitem__>` call, the variable
    ``g`` represents the columns of the joined frame ``X``. In SQL
    this would have been equivalent to ``... JOIN tableX AS g ...``.

    See also
    --------
    - :data:`f` -- main column namespace.
