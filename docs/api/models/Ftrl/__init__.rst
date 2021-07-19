
.. xmethod:: datatable.models.Ftrl.__init__
    :src: src/core/models/py_ftrl.cc Ftrl::m__init__
    :tests: tests/models/test-ftrl.py
    :cvar: doc_models_Ftrl___init__
    :signature: __init__(self, alpha=0.005, beta=1, lambda1=0, lambda2=0, nbins=10**6,
        mantissa_nbits=10, nepochs=1, double_precision=False, negative_class=False,
        interactions=None, model_type='auto', params=None)


    Create a new :class:`Ftrl <datatable.models.Ftrl>` object.

    Parameters
    ----------
    alpha: float
        :math:`\alpha` in per-coordinate FTRL-Proximal algorithm, should be
        positive.

    beta: float
        :math:`\beta` in per-coordinate FTRL-Proximal algorithm, should be non-negative.

    lambda1: float
        L1 regularization parameter, :math:`\lambda_1` in per-coordinate
        FTRL-Proximal algorithm. It should be non-negative.

    lambda2: float
        L2 regularization parameter, :math:`\lambda_2` in per-coordinate
        FTRL-Proximal algorithm. It should be non-negative.

    nbins: int
        Number of bins to be used for the hashing trick, should be positive.

    mantissa_nbits: int
        Number of mantissa bits to take into account when hashing floats.
        It should be non-negative and less than or equal to `52`, that
        is a number of mantissa bits allocated for a C++ 64-bit `double`.

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
        footprint of the `Ftrl` object.

    negative_class: bool
        An option to indicate if a "negative" class should be created
        in the case of multinomial classification. For the "negative"
        class the model will train on all the negatives, and if
        a new label is encountered in the target column, its
        weights will be initialized to the current "negative" class weights.
        If `negative_class` is set to `False`, the initial weights
        become zeros.

    interactions: List[List[str] | Tuple[str]] | Tuple[List[str] | Tuple[str]]
        A list or a tuple of interactions. In turn, each interaction
        should be a list or a tuple of feature names, where each feature
        name is a column name from the training frame. Each interaction
        should have at least one feature.

    model_type: "binomial" | "multinomial" | "regression" | "auto"
        The model type to be built. When this option is `"auto"`
        then the model type will be automatically chosen based on
        the target column `stype`.

    params: FtrlParams
        Named tuple of the above parameters. One can pass either this tuple,
        or any combination of the individual parameters to the constructor,
        but not both at the same time.

    except: ValueError
        The exception is raised if both the `params` and one of the
        individual model parameters are passed at the same time.

