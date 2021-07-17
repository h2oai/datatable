
.. xattr:: datatable.models.Ftrl.feature_importances
    :src: src/core/models/py_ftrl.cc Ftrl::get_fi
    :cvar: doc_models_Ftrl_feature_importances


    Feature importances as calculated during the model training and
    normalized to `[0; 1]`. The normalization is done by dividing
    the accumulated feature importances over the maximum value.

    Parameters
    ----------
    return: Frame
        A frame with two columns: `feature_name` that has stype `str32`,
        and `feature_importance` that has stype `float32` or `float64`
        depending on whether the :attr:`.double_precision`
        option is `False` or `True`.
