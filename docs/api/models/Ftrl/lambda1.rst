
.. xattr:: datatable.models.Ftrl.lambda1
    :src: src/core/models/py_ftrl.cc Ftrl::get_lambda1 Ftrl::set_lambda1
    :cvar: doc_models_Ftrl_lambda1
    :settable: new_lambda1


    L1 regularization parameter, :math:`\lambda_1` in per-coordinate
    FTRL-Proximal algorithm.

    Parameters
    ----------
    return: float
        Current `lambda1` value.

    new_lambda1: float
        New `lambda1` value, should be non-negative.

    except: ValueError
        The exception is raised when `new_lambda1` is negative.
