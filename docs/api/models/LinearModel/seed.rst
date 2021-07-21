
.. xattr:: datatable.models.LinearModel.seed
    :src: src/core/models/py_linearmodel.cc LinearModel::get_seed LinearModel::set_seed
    :cvar: doc_models_LinearModel_seed
    :settable: new_seed


    Seed for the quasi-random number generator that is used for
    data shuffling when fitting the model. If seed is `0`,
    no shuffling is performed.

    Parameters
    ----------
    return: int
        Current `seed` value.

    new_seed: int
        New `seed` value, should be non-negative.
