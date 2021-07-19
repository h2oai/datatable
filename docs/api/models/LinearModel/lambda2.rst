
.. xattr:: datatable.models.LinearModel.lambda2
    :src: src/core/models/py_linearmodel.cc LinearModel::get_lambda2 LinearModel::set_lambda2
    :cvar: doc_models_LinearModel_lambda2
    :settable: new_lambda2


    L2 regularization parameter.

    Parameters
    ----------
    return: float
        Current `lambda2` value.

    new_lambda2: float
        New `lambda2` value, should be non-negative.

    except: ValueError
        The exception is raised when `new_lambda2` is negative.
