
.. xattr:: datatable.models.Ftrl.params
    :src: src/core/models/py_ftrl.cc Ftrl::get_params_namedtuple Ftrl::set_params_namedtuple
    :cvar: doc_models_Ftrl_params
    :settable: new_params


    `Ftrl` model parameters as a named tuple `FtrlParams`,
    see :meth:`.__init__` for more details.
    This option is read-only for a trained model.

    Parameters
    ----------
    return: FtrlParams
        Current `params` value.

    new_params: FtrlParams
        New `params` value.

    except: ValueError
        The exception is raised when

        - trying to change this option for a model that has already been trained;
        - individual parameter values are incompatible with the corresponding setters.
