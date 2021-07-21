
.. xmethod:: datatable.models.Ftrl.predict
    :src: src/core/models/py_ftrl.cc Ftrl::predict
    :cvar: doc_models_Ftrl_predict
    :tests: tests/models/test-ftrl.py
    :signature: predict(self, X)


    Predict for the input samples.

    Parameters
    ----------
    X: Frame
        A frame to make predictions for. It should have the same number
        of columns as the training frame.

    return: Frame
        A new frame of shape `(X.nrows, nlabels)` with the predicted probabilities
        for each row of frame `X` and each of `nlabels` labels
        the model was trained for.

    See also
    --------
    - :meth:`.fit` -- train model on the input samples and targets.
    - :meth:`.reset` -- reset the model.
