
.. xattr:: datatable.Type.void
    :src: --

    The type of a column where all values are NAs.

    In ``datatable``, any column can have NA values in it. There is, however,
    a special type that can be assigned for a column where *all* values are
    NAs: ``void``. This type's special property is that it can be used in
    place where any other type could be expected.


    Examples
    --------
    >>> dt.Frame([None, None, None]).types
    [Type.void]
