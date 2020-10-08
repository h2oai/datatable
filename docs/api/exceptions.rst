
.. xpy:module:: datatable.exceptions


datatable.exceptions
====================

This module contains warnings and exceptions that ``datatable`` may
throw during runtime.

Warnings
--------

Warnings are issued when it is helpful to inform the user of some condition
in a program, that doesn't result in an exception and the program termination.
``datatable`` may issue the following warnings:


.. list-table::
    :widths: auto
    :class: api-table

    * - :exc:`FutureWarning`
      - A built-in python `warning <https://docs.python.org/3/library/exceptions.html#FutureWarning>`_
        about deprecated features.

    * - :exc:`DatatableWarning`
      - A ``datatable`` generic warning.

    * - :exc:`IOWarning`
      - A warning regarding the input/output operation.


Exceptions
----------

Exceptions are thrown when a special condition, that is unexpected,
encountered during runtime. All ``datatable``  exceptions are
descendants of :exc:`DtException` class, so that they can be easily catched.
The following exceptions may be thrown:

.. list-table::
    :widths: auto
    :class: api-table

    * - :exc:`ImportError`
      - https://docs.python.org/3/library/exceptions.html#ImportError

    * - :exc:`IndexError`
      - https://docs.python.org/3/library/exceptions.html#IndexError

    * - :exc:`InvalidOperationError`
      - The operation requested is illegal for the given combination of parameters.

    * - :exc:`IOError`
      - https://docs.python.org/3/library/exceptions.html#IOError

    * - :exc:`KeyError`
      - https://docs.python.org/3/library/exceptions.html#KeyError

    * - :exc:`MemoryError`
      - https://docs.python.org/3/library/exceptions.html#MemoryError

    * - :exc:`NotImplementedError`
      - https://docs.python.org/3/library/exceptions.html#NotImplementedError

    * - :exc:`OverflowError`
      - https://docs.python.org/3/library/exceptions.html#OverflowError

    * - :exc:`TypeError`
      - https://docs.python.org/3/library/exceptions.html#TypeError

    * - :exc:`ValueError`
      - https://docs.python.org/3/library/exceptions.html#ValueError
