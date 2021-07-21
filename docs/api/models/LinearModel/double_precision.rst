
.. xattr:: datatable.models.LinearModel.double_precision
    :src: src/core/models/py_linearmodel.cc LinearModel::get_double_precision
    :cvar: doc_models_LinearModel_double_precision


    An option to indicate whether double precision, i.e. `float64`,
    or single precision, i.e. `float32`, arithmetic should be
    used for computations. This option is read-only and can only be set
    during the `LinearModel` object :meth:`construction <datatable.models.LinearModel.__init__>`.

    Parameters
    ----------
    return: bool
        Current `double_precision` value.
