
.. xattr:: datatable.models.LinearModel.params
    :src: src/core/models/py_linearmodel.cc LinearModel::get_params_namedtuple LinearModel::set_params_namedtuple
    :cvar: doc_models_LinearModel_params
    :settable: new_params

    `LinearModel` model parameters as a named tuple `LinearModelParams`,
    see :meth:`.__init__` for more details.
    This option is read-only for a trained model.

    Parameters
    ----------
    return: LinearModelParams
        Current `params` value.

    new_params: LinearModelParams
        New `params` value.

    except: ValueError
        The exception is raised when

        - trying to change this option for a model that has already been trained;
        - individual parameter values are incompatible with the corresponding setters.

