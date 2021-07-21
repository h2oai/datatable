
.. xattr:: datatable.models.LinearModel.eta_schedule
    :src: src/core/models/py_linearmodel.cc LinearModel::get_eta_schedule LinearModel::set_eta_schedule
    :cvar: doc_models_LinearModel_eta_schedule
    :settable: new_eta_schedule


    Learning rate `schedule <https://en.wikipedia.org/wiki/Learning_rate#Learning_rate_schedule>`_

    - `"constant"` for constant `eta`;
    - `"time-based"` for time-based schedule;
    - `"step-based"` for step-based schedule;
    - `"exponential"` for exponential schedule.

    Parameters
    ----------
    return: str
        Current `eta_schedule` value.

    new_eta_schedule: "constant" | "time-based" | "step-based" | "exponential"
        New `eta_schedule` value.
