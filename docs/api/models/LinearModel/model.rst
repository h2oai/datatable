
.. xattr:: datatable.models.LinearModel.model
    :src: src/core/models/py_linearmodel.cc LinearModel::get_model
    :cvar: doc_models_LinearModel_model


    Trained models coefficients.

    Parameters
    ----------
    return: Frame
        A frame of shape `(nfeatures + 1, nlabels)`, where `nlabels` is
        the number of labels the model was trained on, and
        `nfeatures` is the number of features. Each column contains
        model coefficients for the corresponding label: starting from
        the intercept and following by the coefficients for each of
        of the `nfeatures` features.
