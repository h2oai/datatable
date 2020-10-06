
.. xdata:: datatable.exceptions
    :src: --

    This namespace contains warnings and exceptions that ``datatable`` may
    throw during runtime.

    Warnings are issued when it is helpful to inform the user of some condition
    in a program, that doesn't result in an exception and the program termination.
    Along with the built-in Python `warnings <https://docs.python.org/3/library/warnings.html>`_
    ``datatable`` may also issue:

    .. list-table::
        :widths: auto
        :class: api-table

        * - ``DatatableWarning``
          - Generic ``datatable`` warnings;

        * - ``FreadWarning``
          - Warnings related to :func:`fread`;

        * - `IOWarning`
          - Warnings related to input/output.


    Exceptions are thrown when a special condition, that is unexpected or
    anomalous, encountered during runtime. Along with the built-in Python
    `exceptions <https://docs.python.org/3/library/exceptions.html>`_
    ``datatable`` may also throw:

    .. list-table::
        :widths: auto
        :class: api-table

        * - `InvalidOperationError`
          - Thrown when the operation requested is illegal
            for the given combination of parameters.

