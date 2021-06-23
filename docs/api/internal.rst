
.. xpy:module:: datatable.internal

datatable.internal
==================

.. warning::

    The functions in this sub-module are considered to be "internal" and
    not useful for day-to-day work with :mod:`datatable` module.

.. list-table::
    :widths: auto
    :class: api-table

    * - :func:`frame_column_data_r()`
      - C pointer to column's data

    * - :func:`frame_columns_virtual()`
      - Indicators of which columns in the frame are virtual.

    * - :func:`frame_integrity_check()`
      - Run checks on whether the frame's state is corrupted.

    * - :func:`get_thread_ids()`
      - Get ids of threads spawned by datatable.



.. toctree::
    :hidden:

    frame_column_data_r()    <internal/frame_column_data_r>
    frame_columns_virtual()  <internal/frame_columns_virtual>
    frame_integrity_check()  <internal/frame_integrity_check>
    get_thread_ids()         <internal/get_thread_ids>
