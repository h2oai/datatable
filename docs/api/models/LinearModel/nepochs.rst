
.. xattr:: datatable.models.LinearModel.nepochs
    :src: src/core/models/py_linearmodel.cc LinearModel::get_nepochs LinearModel::set_nepochs
    :cvar: doc_models_LinearModel_nepochs
    :settable: new_nepochs


    Number of training epochs. When `nepochs` is an integer number,
    the model will train on all the data provided to :meth:`.fit` method
    `nepochs` times. If `nepochs` has a fractional part `{nepochs}`,
    the model will train on all the data `[nepochs]` times,
    i.e. the integer part of `nepochs`. Plus, it will also perform an additional
    training iteration on the `{nepochs}` fraction of data.

    Parameters
    ----------
    return: float
        Current `nepochs` value.

    new_nepochs: float
        New `nepochs` value, should be non-negative.

    except: ValueError
        The exception is raised when `new_nepochs` value is negative.
