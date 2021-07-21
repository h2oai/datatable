
.. xattr:: datatable.models.Ftrl.model
    :src: src/core/models/py_ftrl.cc Ftrl::get_model
    :cvar: doc_models_Ftrl_model


    Trained models weights, i.e. `z` and `n` coefficients
    in per-coordinate FTRL-Proximal algorithm.

    Parameters
    ----------
    return: Frame
        A frame of shape `(nbins, 2 * nlabels)`, where `nlabels` is
        the total number of labels the model was trained on, and
        :attr:`nbins <datatable.models.Ftrl.nbins>` is the number of bins
        used for the hashing trick. Odd and even columns represent
        the `z` and `n` model coefficients, respectively.
