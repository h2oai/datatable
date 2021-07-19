
.. xattr:: datatable.models.LinearModel.eta_drop_rate
    :src: src/core/models/py_linearmodel.cc LinearModel::get_eta_drop_rate LinearModel::set_eta_drop_rate
    :cvar: doc_models_LinearModel_eta_drop_rate
    :settable: new_eta_drop_rate


    Drop rate for the `"step-based"` learning rate schedule.

    Parameters
    ----------
    return: float
        Current `eta_drop_rate` value.

    new_eta_drop_rate: float
        New `eta_drop_rate` value, should be positive.

    except: ValueError
        The exception is raised when `new_eta_drop_rate` is not positive.
