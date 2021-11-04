
.. xfunction:: datatable.internal.get_thread_ids
    :src: src/core/datatablemodule.cc get_thread_ids
    :cvar: doc_internal_get_thread_ids
    :signature: get_thread_ids()

    Return system ids of all threads used internally by datatable.

    Calling this function will cause the threads to spawn if they
    haven't done already. (This behavior may change in the future).


    Parameters
    ----------
    return: List[str]
        The list of thread ids used by the datatable. The first element
        in the list is the id of the main thread.


    See Also
    --------
    - :attr:`dt.options.nthreads` -- global option that controls the
      number of threads in use.
