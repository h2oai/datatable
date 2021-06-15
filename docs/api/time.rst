
.. xpy:module:: datatable.time

datatable.time
==============

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`day()`
     - Return day component of a date.

   * - :func:`day_of_week()`
     - Compute day of week for the given date.

   * - :func:`month()`
     - Return month component of a date.

   * - :func:`year()`
     - Return year component of a date.

   * - :func:`ymd(y,m,d)`
     - Create a ``date32`` column from year, month, and day components.

   * - :func:`ymdt(y,m,d,H,M,S)`
     - Create a ``time64`` column from year, month, day, hour, minute, and
       second components.


.. toctree::
    :hidden:

    day()          <time/day>
    day_of_week()  <time/day_of_week>
    month()        <time/month>
    year()         <time/year>
    ymd()          <time/ymd>
    ymdt()         <time/ymdt>
