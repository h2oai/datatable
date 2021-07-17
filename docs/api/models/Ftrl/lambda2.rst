
.. xattr:: datatable.models.Ftrl.lambda2
    :src: src/core/models/py_ftrl.cc Ftrl::get_lambda2 Ftrl::set_lambda2
    :cvar: doc_models_Ftrl_lambda2
    :settable: new_lambda2


    L2 regularization parameter, :math:`\lambda_2` in per-coordinate
    FTRL-Proximal algorithm.

    Parameters
    ----------
    return: float
        Current `lambda2` value.

    new_lambda2: float
        New `lambda2` value, should be non-negative.

    except: ValueError
        The exception is raised when `new_lambda2` is negative.
