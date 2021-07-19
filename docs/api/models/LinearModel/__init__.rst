
.. xmethod:: datatable.models.LinearModel.__init__
    :src: src/core/models/py_linearmodel.cc LinearModel::m__init__
    :tests: tests/models/test-linearmodel.py
    :cvar: doc_models_LinearModel___init__
    :signature: __init__(self,
        eta0=0.005, eta_decay=0.0001, eta_drop_rate=10.0, eta_schedule='constant',
        lambda1=0, lambda2=0, nepochs=1, double_precision=False, negative_class=False,
        model_type='auto', seed=0, params=None)


    Create a new :class:`LinearModel <datatable.models.LinearModel>` object.

    Parameters
    ----------
    eta0: float
        The initial learning rate, should be positive.

    eta_decay: float
        Decay for the `"time-based"` and `"step-based"`
        learning rate schedules, should be non-negative.

    eta_drop_rate: float
        Drop rate for the `"step-based"` learning rate schedule,
        should be positive.

    eta_schedule: "constant" | "time-based" | "step-based" | "exponential"
        Learning rate schedule. When it is `"constant"` the learning rate
        `eta` is constant and equals to `eta0`. Otherwise,
        after each training iteration `eta` is updated as follows:

        - for `"time-based"` schedule as `eta0 / (1 + eta_decay * epoch)`;
        - for `"step-based"` schedule as `eta0 * eta_decay ^ floor((1 + epoch) / eta_drop_rate)`;
        - for `"exponential"` schedule as `eta0 / exp(eta_decay * epoch)`.

        By default, the size of the training iteration is one epoch, it becomes
        `nepochs_validation` when validation dataset is specified.

    lambda1: float
        L1 regularization parameter, should be non-negative.

    lambda2: float
        L2 regularization parameter, should be non-negative.

    nepochs: float
        Number of training epochs, should be non-negative. When `nepochs`
        is an integer number, the model will train on all the data
        provided to :meth:`.fit` method `nepochs` times. If `nepochs`
        has a fractional part `{nepochs}`, the model will train on all
        the data `[nepochs]` times, i.e. the integer part of `nepochs`.
        Plus, it will also perform an additional training iteration
        on the `{nepochs}` fraction of data.

    double_precision: bool
        An option to indicate whether double precision, i.e. `float64`,
        or single precision, i.e. `float32`, arithmetic should be used
        for computations. It is not guaranteed that setting
        `double_precision` to `True` will automatically improve
        the model accuracy. It will, however, roughly double the memory
        footprint of the `LinearModel` object.

    negative_class: bool
        An option to indicate if a "negative" class should be created
        in the case of multinomial classification. For the "negative"
        class the model will train on all the negatives, and if
        a new label is encountered in the target column, its
        coefficients will be initialized to the current "negative" class coefficients.
        If `negative_class` is set to `False`, the initial coefficients
        become zeros.

    model_type: "binomial" | "multinomial" | "regression" | "auto"
        The model type to be built. When this option is `"auto"`
        then the model type will be automatically chosen based on
        the target column `stype`.

    seed: int
        Seed for the quasi-random number generator that is used for
        data shuffling when fitting the model, should be non-negative.
        If seed is zero, no shuffling is performed.

    params: LinearModelParams
        Named tuple of the above parameters. One can pass either this tuple,
        or any combination of the individual parameters to the constructor,
        but not both at the same time.

    except: ValueError
        The exception is raised if both the `params` and one of the
        individual model parameters are passed at the same time.
