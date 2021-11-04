
.. xattr:: datatable.models.LinearModel.eta0
    :src: src/core/models/py_linearmodel.cc LinearModel::get_eta0 LinearModel::set_eta0
    :cvar: doc_models_LinearModel_eta0
    :settable: new_eta0


    Learning rate.

    Parameters
    ----------
    return: float
        Current `eta0` value.

    new_eta0: float
        New `eta0` value, should be positive.

    except: ValueError
        The exception is raised when `new_eta0` is not positive.
