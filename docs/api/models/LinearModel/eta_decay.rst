
.. xattr:: datatable.models.LinearModel.eta_decay
    :src: src/core/models/py_linearmodel.cc LinearModel::get_eta_decay LinearModel::set_eta_decay
    :cvar: doc_models_LinearModel_eta_decay
    :settable: new_eta_decay


    Decay for the `"time-based"` and `"step-based"` learning rate schedules.

    Parameters
    ----------
    return: float
        Current `eta_decay` value.

    new_eta_decay: float
        New `eta_decay` value, should be non-negative.

    except: ValueError
        The exception is raised when `new_eta_decay` is negative.
