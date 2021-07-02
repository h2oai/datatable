
.. xmethod:: datatable.Frame.export_names
    :src: src/core/frame/py_frame.cc Frame::export_names
    :cvar: doc_Frame_export_names
    :signature: export_names(self)

    .. x-version-added:: 0.10

    Return a tuple of :ref:`f-expressions` for all columns of the frame.

    For example, if the frame has columns "A", "B", and "C", then this
    method will return a tuple of expressions ``(f.A, f.B, f.C)``. If you
    assign these to, say, variables ``A``, ``B``, and ``C``, then you
    will be able to write column expressions using the column names
    directly, without using the ``f`` symbol::

        >>> A, B, C = DT.export_names()
        >>> DT[A + B > C, :]

    The variables that are "exported" refer to each column *by name*. This
    means that you can use the variables even after reordering the
    columns. In addition, the variables will work not only for the frame
    they were exported from, but also for any other frame that has columns
    with the same names.

    Parameters
    ----------
    return: Tuple[Expr, ...]
        The length of the tuple is equal to the number of columns in the
        frame. Each element of the tuple is a datatable *expression*, and
        can be used primarily with the ``DT[i,j]`` notation.

    Notes
    -----
    - This method is effectively equivalent to::

        >>> def export_names(self):
        ...     return tuple(f[name] for name in self.names)

    - If you want to export only a subset of column names, then you can
      either subset the frame first, or use ``*``-notation to ignore the
      names that you do not plan to use::

        >>> A, B = DT[:, :2].export_names()  # export the first two columns
        >>> A, B, *_ = DT.export_names()     # same

    - Variables that you use in code do not have to have the same names
      as the columns::

        >>> Price, Quantity = DT[:, ["sale price", "quant"]].export_names()
