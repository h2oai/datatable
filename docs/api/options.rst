
.. xdata:: datatable.options
    :src: src/datatable/__init__.py options

    This property controls the following datatable option groups:

    .. list-table::
        :widths: auto
        :class: api-table

        * - :data:`.debug <datatable.options.debug>`
          - Debug options.

        * - :data:`.display <datatable.options.display>`
          - Display options.

        * - :data:`.frame <datatable.options.frame>`
          - :class:`Frame` related options.

        * - :data:`.fread <datatable.options.fread>`
          - :func:`fread()` related options.

        * - :data:`.progress <datatable.options.progress>`
          - Progress reporting options.

    It also controls the following individual options:

    .. list-table::
        :widths: auto
        :class: api-table

        * - :data:`.nthreads <datatable.options.nthreads>`
          - Number of threads used by datatable for parallel computations.

.. toctree::
    :hidden:

    debug      <options/debug>
    display    <options/display>
    frame      <options/frame>
    fread      <options/fread>
    progress   <options/progress>
    nthreads   <options/nthreads>
