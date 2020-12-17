
.. xmethod:: datatable.FExpr.len
    :src: src/core/expr/fexpr.cc PyFExpr::len

    len(self)
    --

    .. x-version-deprecated:: 0.11

    Return the string length for a string column. This method can only
    be applied to string columns, and it returns an integer column as a
    result.

    Since version 1.0 this function will be available in the ``str.``
    module.
