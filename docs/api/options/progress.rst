
.. xclass:: datatable.options.progress
    :qual-type: class
    :src: --

    This namespace contains the following progress reporting options:

    .. list-table::
        :widths: auto
        :class: api-table

        * - :attr:`.allow_interruption <datatable.options.progress.allow_interruption>`
          - Option that controls if the datatable tasks could be interrupted.

        * - :attr:`.callback <datatable.options.progress.callback>`
          - A custom progress-reporting function.

        * - :attr:`.clear_on_success <datatable.options.progress.clear_on_success>`
          - Option that controls if the progress bar is cleared on success.

        * - :attr:`.enabled <datatable.options.progress.enabled>`
          - Option that controls if the progress reporting is enabled.

        * - :attr:`.min_duration <datatable.options.progress.min_duration>`
          - The minimum duration of a task to show the progress bar.

        * - :attr:`.updates_per_second <datatable.options.progress.updates_per_second>`
          - The progress bar update frequency.


.. toctree::
     :hidden:

     allow_interruption   <progress/allow_interruption>
     callback             <progress/callback>
     clear_on_success     <progress/clear_on_success>
     enabled              <progress/enabled>
     min_duration         <progress/min_duration>
     updates_per_second   <progress/updates_per_second>

