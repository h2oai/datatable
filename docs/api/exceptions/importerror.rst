
.. xexc:: datatable.exceptions.ImportError
    :src: --

    This exception may be raised when a datatable operation requires an external
    module or library, but that module is not available. Examples of such
    operations include: converting a Frame into a pandas DataFrame, or into an
    Arrow Table, or reading an Excel file.

    Inherits from Python :py:exc:`ImportError` and :exc:`datatable.exceptions.DtException`.
