
.. xmethod:: datatable.Namespace.__getattribute__
    :src: src/core/expr/namespace.cc Namespace::m__getattr__

    __getattribute__(self, name)
    --

    Retrieve a column from the namespace by `name`.

    This is a convenience form that can be used to access simply-named
    columns. For example: ``f.Age`` denotes a column called ``"Age"``,
    and is exactly equivalent to ``f['Age']``.


    Parameters
    ----------
    name: str
        Name of the column to select.

    return: FExpr
        An expression that selects the specified column from a frame.


    See also
    --------
    - :meth:`.__getitem__()` -- retrieving columns
      via the ``[]``-notation.
