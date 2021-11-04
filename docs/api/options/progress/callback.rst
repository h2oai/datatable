
.. xattr:: datatable.options.progress.callback
    :src: src/core/progress/_options.cc get_callback set_callback
    :cvar: doc_options_progress_callback
    :settable: value

    This option controls the custom progress-reporting function.


    Parameters
    ----------
    return: function
        Current `callback` value. Initially, this option is set to `None`.

    value: function
        New `callback` value. If `None`, then the built-in progress-reporting
        function will be used. Otherwise, the `value` specifies a function
        to be called at each progress event. The function should take a single
        parameter `p`, which is a namedtuple with the following fields:

        - `p.progress` is a float in the range `0.0 .. 1.0`;
        - `p.status` is a string, one of `'running'`, `'finished'`, `'error'` or
          `'cancelled'`;
        - `p.message` is a custom string describing the operation currently
          being performed.
