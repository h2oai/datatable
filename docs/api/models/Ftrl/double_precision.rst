
.. xattr:: datatable.models.Ftrl.double_precision
    :src: src/core/models/py_ftrl.cc Ftrl::get_double_precision
    :cvar: doc_models_Ftrl_double_precision


    An option to indicate whether double precision, i.e. `float64`,
    or single precision, i.e. `float32`, arithmetic should be
    used for computations. This option is read-only and can only be set
    during the `Ftrl` object :meth:`construction <datatable.models.Ftrl.__init__>`.

    Parameters
    ----------
    return: bool
        Current `double_precision` value.
