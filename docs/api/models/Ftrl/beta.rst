
.. xattr:: datatable.models.Ftrl.beta
    :src: src/core/models/py_ftrl.cc Ftrl::get_beta Ftrl::set_beta
    :cvar: doc_models_Ftrl_beta
    :settable: new_beta


    :math:`\beta` in per-coordinate FTRL-Proximal algorithm.

    Parameters
    ----------
    return: float
        Current `beta` value.

    new_beta: float
        New `beta` value, should be non-negative.

    except: ValueError
        The exception is raised when `new_beta` is negative.
