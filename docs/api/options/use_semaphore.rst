
.. xattr:: datatable.options.use_semaphore
    :src: src/core/parallel/thread_pool.cc get_use_semaphore set_use_semaphore
    :cvar: doc_options_use_semaphore
    :settable: new_use_semaphore

    This experimental option controls whether datatable should
    use semaphore or condition variable for threads waiting.
    In the former case, datatable will demonstrate better performance,
    however, may monopolize CPU, so on shared systems it is recommended
    to set this option to `False`.

    Parameters
    ----------
    return: bool
        When `True`, datatable uses semaphore for threads waiting.
        When `False`, condition variable is used.

    new_use_semaphore: bool
        New `use_semaphore` value.
