
.. xmethod:: datatable.models.LinearModel.fit
    :src: src/core/models/py_linearmodel.cc LinearModel::fit
    :tests: tests/models/test-linearmodel.py
    :cvar: doc_models_LinearModel_fit
    :signature: fit(self, X_train, y_train, X_validation=None, y_validation=None,
        nepochs_validation=1, validation_error=0.01,
        validation_average_niterations=1)


    Train model on the input samples and targets using the
    `parallel stochastic gradient descent <http://martin.zinkevich.org/publications/nips2010.pdf>`_
    method.

    Parameters
    ----------
    X_train: Frame
        Training frame.

    y_train: Frame
        Target frame having as many rows as `X_train` and one column.

    X_validation: Frame
        Validation frame having the same number of columns as `X_train`.

    y_validation: Frame
        Validation target frame of shape `(nrows, 1)`.

    nepochs_validation: float
        Parameter that specifies how often, in epoch units, validation
        error should be checked.

    validation_error: float
        The improvement of the relative validation error that should be
        demonstrated by the model within `nepochs_validation` epochs,
        otherwise the training will stop.

    validation_average_niterations: int
        Number of iterations that is used to average the validation error.
        Each iteration corresponds to `nepochs_validation` epochs.

    return: LinearModelFitOutput
        `LinearModelFitOutput` is a `Tuple[float, float]` with two fields: `epoch` and `loss`,
        representing the final fitting epoch and the final loss, respectively.
        If validation dataset is not provided, the returned `epoch` equals to
        `nepochs` and the `loss` is just `float('nan')`.

    See also
    --------
    - :meth:`.predict` -- predict for the input samples.
    - :meth:`.reset` -- reset the model.
