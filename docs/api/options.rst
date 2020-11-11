
.. xclass:: datatable.options
    :src: src/datatable/options.py Config

    This namespace contains the following option groups:

    .. list-table::
        :widths: auto
        :class: api-table

        * - :class:`.debug <datatable.options.debug>`
          - Debug options.

        * - :class:`.display <datatable.options.display>`
          - Display options.

        * - :class:`.frame <datatable.options.frame>`
          - :class:`Frame <dt.Frame>`-related options.

        * - :class:`.fread <datatable.options.fread>`
          - :func:`fread() <dt.fread>`-related options.

        * - :class:`.progress <datatable.options.progress>`
          - Progress reporting options.

    It also contains the following individual options:

    .. list-table::
        :widths: auto
        :class: api-table

        * - :attr:`.nthreads <datatable.options.nthreads>`
          - Number of threads used by datatable for parallel computations.

.. toctree::
    :hidden:

    debug      <options/debug>
    display    <options/display>
    frame      <options/frame>
    fread      <options/fread>
    progress   <options/progress>
    nthreads   <options/nthreads>
