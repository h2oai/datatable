
.. xattr:: datatable.models.LinearModel.lambda1
    :src: src/core/models/py_linearmodel.cc LinearModel::get_lambda1 LinearModel::set_lambda1
    :cvar: doc_models_LinearModel_lambda1
    :settable: new_lambda1


    L1 regularization parameter.

    Parameters
    ----------
    return: float
        Current `lambda1` value.

    new_lambda1: float
        New `lambda1` value, should be non-negative.

    except: ValueError
        The exception is raised when `new_lambda1` is negative.
