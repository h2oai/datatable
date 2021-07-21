
.. xattr:: datatable.models.LinearModel.labels
    :src: src/core/models/py_linearmodel.cc LinearModel::get_labels
    :cvar: doc_models_LinearModel_labels


    Classification labels the model was trained on.

    Parameters
    ----------
    return: Frame
        A one-column frame with the classification labels.
        In the case of numeric regression, the label is
        the target column name.
