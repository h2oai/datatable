
.. xattr:: datatable.models.Ftrl.alpha
    :src: src/core/models/py_ftrl.cc Ftrl::get_alpha Ftrl::set_alpha
    :cvar: doc_models_Ftrl_alpha
    :settable: new_alpha


    :math:`\alpha` in per-coordinate FTRL-Proximal algorithm.

    Parameters
    ----------
    return: float
        Current `alpha` value.

    new_alpha: float
        New `alpha` value, should be positive.

    except: ValueError
        The exception is raised when `new_alpha` is not positive.
