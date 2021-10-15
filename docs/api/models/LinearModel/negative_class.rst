
.. xattr:: datatable.models.LinearModel.negative_class
    :src: src/core/models/py_linearmodel.cc LinearModel::get_negative_class LinearModel::set_negative_class
    :cvar: doc_models_LinearModel_negative_class
    :settable: new_negative_class


    An option to indicate if a "negative" class should be created
    in the case of multinomial classification. For the "negative"
    class the model will train on all the negatives, and if
    a new label is encountered in the target column, its
    coefficients are initialized to the current "negative" class coefficients.
    If `negative_class` is set to `False`, the initial coefficients
    become zeros.

    This option is read-only for a trained model.

    Parameters
    ----------
    return: bool
        Current `negative_class` value.

    new_negative_class: bool
        New `negative_class` value.

    except: ValueError
        The exception is raised when trying to change this option
        for a model that has already been trained.
