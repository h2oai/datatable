
.. xdata:: datatable.options.progress
    :src: src/datatable/__init__.py options

    This property controls the following progress reporting options:

    .. list-table::
        :widths: auto
        :class: api-table

        * - :data:`.allow_interruption <datatable.options.progress.allow_interruption>`
          - Switch that controls if the datatable tasks could be interrupted.

        * - :data:`.callback <datatable.options.progress.callback>`
          - A custom progress-reporting function.

        * - :data:`.clear_on_success <datatable.options.progress.clear_on_success>`
          - Switch that controls if the progress bar is cleared on success.

        * - :data:`.enabled <datatable.options.progress.enabled>`
          - Switch that controls if the progress reporting is enabled.

        * - :data:`.min_duration <datatable.options.progress.min_duration>`
          - The minimum duration of a task to show the progress bar.

        * - :data:`.updates_per_second <datatable.options.progress.updates_per_second>`
          - The progress bar update frequency.


.. toctree::
     :hidden:

     allow_interruption   <progress/allow_interruption>
     callback             <progress/callback>
     clear_on_success     <progress/clear_on_success>
     enabled              <progress/enabled>
     min_duration         <progress/min_duration>
     updates_per_second   <progress/updates_per_second>

