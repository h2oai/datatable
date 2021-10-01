
.. xattr:: datatable.options.nthreads
    :src: src/core/parallel/thread_pool.cc get_nthreads set_nthreads
    :cvar: doc_options_nthreads
    :settable: new_nthreads

    This option controls the number of threads used by datatable
    for parallel calculations.

    Parameters
    ----------
    return: int
        Current `nthreads` value. Initially, this option is set to
        the value returned by C++ call `std::thread::hardware_concurrency()`,
        and usually equals to the number of available cores.

    new_nthreads: int
        New `nthreads` value. It can be greater or smaller than the initial setting.
        For example, setting `nthreads = 1` will force the library into
        a single-threaded mode. Setting `nthreads` to `0` will restore
        the initial value equal to the number of processor cores.
        Setting `nthreads` to a value less than `0` is equivalent to requesting
        that fewer threads than the maximum.
