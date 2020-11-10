
.. xexc:: datatable.exceptions.OverflowError
    :src: --

    Rare error that may occur if you pass a parameter that is too large to fit
    into C++ `int64` type, or sometimes larger than a `double`.

    Inherits from Python :py:exc:`OverflowError` and :exc:`datatable.exceptions.DtException`.
