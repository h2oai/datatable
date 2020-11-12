
.. xexc:: datatable.exceptions.InvalidOperationError
    :src: --

    Raised in multiple scenarios whenever the requested operation is
    logically invalid with the given combination of parameters.

    For example, :meth:`cbind <dt.Frame.cbind>`-ing several frames with
    incompatible shapes.

    Inherits from :exc:`datatable.exceptions.DtException`.
