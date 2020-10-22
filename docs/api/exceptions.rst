
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
      - inherited from the built-in
        `ImportError <https://docs.python.org/3/library/exceptions.html#ImportError>`_.

    * - :exc:`IndexError`
      - inherited from the built-in
        `IndexError <https://docs.python.org/3/library/exceptions.html#IndexError>`_.

    * - :exc:`InvalidOperationError`
      - The operation requested is illegal for the given combination of parameters.

    * - :exc:`IOError`
      - inherited from the built-in
        `IOError <https://docs.python.org/3/library/exceptions.html#IOError>`_.

    * - :exc:`KeyError`
      - inherited from the built-in
        `KeyError <https://docs.python.org/3/library/exceptions.html#KeyError>`_.

    * - :exc:`MemoryError`
      - inherited from the built-in
        `MemoryError <https://docs.python.org/3/library/exceptions.html#MemoryError>`_.

    * - :exc:`NotImplementedError`
      - inherited from the built-in
        `NotImplementedError <https://docs.python.org/3/library/exceptions.html#NotImplementedError>`_.

    * - :exc:`OverflowError`
      - inherited from the built-in
        `OverflowError <https://docs.python.org/3/library/exceptions.html#OverflowError>`_.

    * - :exc:`TypeError`
      - inherited from the built-in
        `TypeError <https://docs.python.org/3/library/exceptions.html#TypeError>`_.

    * - :exc:`ValueError`
      - inherited from the built-in
        `ValueError <https://docs.python.org/3/library/exceptions.html#ValueError>`_.
