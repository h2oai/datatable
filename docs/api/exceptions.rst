
.. xpy:module:: datatable.exceptions


datatable.exceptions
====================

This module contains warnings and exceptions that :mod:`datatable` may
throw during runtime.



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
      - Equivalient to the built-in :py:exc:`ImportError`.

    * - :exc:`IndexError`
      - Equivalient to the built-in :py:exc:`IndexError`.

    * - :exc:`InvalidOperationError`
      - The operation requested is illegal for the given combination of parameters.

    * - :exc:`IOError`
      - Equivalient to the built-in :py:exc:`IOError`.

    * - :exc:`KeyError`
      - Equivalient to the built-in :py:exc:`KeyError`.

    * - :exc:`MemoryError`
      - Equivalient to the built-in :py:exc:`MemoryError`.

    * - :exc:`NotImplementedError`
      - Equivalient to the built-in :py:exc:`NotImplementedError`.

    * - :exc:`OverflowError`
      - Equivalient to the built-in :py:exc:`OverflowError`.

    * - :exc:`TypeError`
      - Equivalient to the built-in :py:exc:`TypeError`.

    * - :exc:`ValueError`
      - Equivalient to the built-in :py:exc:`ValueError`.



Warnings
--------

Warnings are issued when it is helpful to inform the user of some condition
in a program, that doesn't result in an exception and the program termination.
We may issue the following warnings:


.. list-table::
    :widths: auto
    :class: api-table

    * - :py:exc:`FutureWarning`
      - A built-in python warning about deprecated features.

    * - :exc:`DatatableWarning`
      - A ``datatable`` generic warning.

    * - :exc:`IOWarning`
      - A warning regarding the input/output operation.



.. toctree::
    :hidden:

    DtException            <exceptions/dtexception>
    DtWarning              <exceptions/dtwarning>
    ImportError            <exceptions/importerror>
    InvalidOperationError  <exceptions/invalidoperationerror>
    IndexError             <exceptions/indexerror>
    IOError                <exceptions/ioerror>
    IOWarning              <exceptions/iowarning>
    KeyError               <exceptions/keyerror>
    MemoryError            <exceptions/memoryerror>
    NotImplementedError    <exceptions/notimplementederror>
    OverflowError          <exceptions/overflowerror>
    TypeError              <exceptions/typeerror>
    ValueError             <exceptions/valueerror>
