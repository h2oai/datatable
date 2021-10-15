
.. xclass:: datatable.models.Ftrl
    :src: src/core/models/py_ftrl.h Ftrl
    :cvar: doc_models_Ftrl
    :tests: tests/models/test-ftrl.py


    This class implements the Follow the Regularized Leader (FTRL) model,
    that is based on the
    `FTRL-Proximal <https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf>`_
    online learning algorithm for binomial logistic regression. Multinomial
    classification and regression for continuous targets are also implemented,
    though these implementations are experimental. This model is fully parallel
    and is based on the
    `Hogwild approach <https://people.eecs.berkeley.edu/~brecht/papers/hogwildTR.pdf>`_
    for parallelization.

    The model supports numerical (boolean, integer and float types),
    temporal (date and time types) and string features. To vectorize features a hashing trick
    is employed, such that all the values are hashed with the 64-bit hashing function.
    This function is implemented as follows:

    - for booleans and integers the hashing function is essentially an identity
      function;

    - for floats the hashing function trims mantissa, taking into account
      :attr:`mantissa_nbits <datatable.models.Ftrl.mantissa_nbits>`,
      and interprets the resulting bit representation as a 64-bit unsigned integer;

    - for date and time types the hashing function is essentially an identity
      function that is based on their internal integer representations;

    - for strings the 64-bit `Murmur2 <https://github.com/aappleby/smhasher>`_
      hashing function is used.

    To compute the final hash `x` the Murmur2 hashed feature name is added
    to the hashed feature and the result is modulo divided by the number of
    requested bins, i.e. by :attr:`nbins <datatable.models.Ftrl.nbins>`.

    For each hashed row of data, according to
    `Ad Click Prediction: a View from the Trenches <https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf>`_,
    the following FTRL-Proximal algorithm is employed:

    .. raw:: html

          <img src="../../_static/ftrl_algorithm.png" width="400"
           alt="Per-coordinate FTRL-Proximal online learning algorithm" />

    When trained, the model can be used to make predictions, or it can be
    re-trained on new datasets as many times as needed improving
    model weights from run to run.


    Construction
    ------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`Ftrl() <datatable.models.Ftrl.__init__>`
          - Construct an `Ftrl` object.



    Methods
    -------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`fit() <datatable.models.Ftrl.fit>`
          - Train model on the input samples and targets.

        * - :meth:`predict() <datatable.models.Ftrl.predict>`
          - Predict for the input samples.

        * - :meth:`reset() <datatable.models.Ftrl.reset>`
          - Reset the model.



    Properties
    ----------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :attr:`alpha <datatable.models.Ftrl.alpha>`
          - :math:`\alpha` in per-coordinate FTRL-Proximal algorithm.

        * - :attr:`beta <datatable.models.Ftrl.beta>`
          - :math:`\beta` in per-coordinate FTRL-Proximal algorithm.

        * - :attr:`colnames <datatable.models.Ftrl.colnames>`
          - Column names of the training frame, i.e. features.

        * - :attr:`colname_hashes <datatable.models.Ftrl.colname_hashes>`
          - Hashes of the column names.

        * - :attr:`double_precision <datatable.models.Ftrl.double_precision>`
          - An option to control precision of the internal computations.

        * - :attr:`feature_importances <datatable.models.Ftrl.feature_importances>`
          - Feature importances calculated during training.

        * - :attr:`interactions <datatable.models.Ftrl.interactions>`
          - Feature interactions.

        * - :attr:`labels <datatable.models.Ftrl.labels>`
          - Classification labels.

        * - :attr:`lambda1 <datatable.models.Ftrl.lambda2>`
          -  L1 regularization parameter, :math:`\lambda_1` in per-coordinate FTRL-Proximal algorithm.

        * - :attr:`lambda2 <datatable.models.Ftrl.lambda2>`
          -  L2 regularization parameter, :math:`\lambda_2` in per-coordinate FTRL-Proximal algorithm.

        * - :attr:`mantissa_nbits <datatable.models.Ftrl.mantissa_nbits>`
          - Number of mantissa bits for hashing floats.

        * - :attr:`model <datatable.models.Ftrl.model>`
          - The model's `z` and `n` coefficients.

        * - :attr:`model_type <datatable.models.Ftrl.model_type>`
          - A model type `Ftrl` should build.

        * - :attr:`model_type_trained <datatable.models.Ftrl.model_type_trained>`
          - A model type `Ftrl` has built.

        * - :attr:`nbins <datatable.models.Ftrl.nbins>`
          - Number of bins for the hashing trick.

        * - :attr:`negative_class <datatable.models.Ftrl.negative_class>`
          - An option to indicate if the "negative" class should be a created
            for multinomial classification.

        * - :attr:`nepochs <datatable.models.Ftrl.nepochs>`
          - Number of training epochs.

        * - :attr:`params <datatable.models.Ftrl.params>`
          - All the input model parameters as a named tuple.


.. toctree::
    :hidden:

    .__init__()               <Ftrl/__init__>
    .alpha                    <Ftrl/alpha>
    .beta                     <Ftrl/beta>
    .colnames                 <Ftrl/colnames>
    .colname_hashes           <Ftrl/colname_hashes>
    .double_precision         <Ftrl/double_precision>
    .feature_importances      <Ftrl/feature_importances>
    .fit()                    <Ftrl/fit>
    .interactions             <Ftrl/interactions>
    .labels                   <Ftrl/labels>
    .lambda1                  <Ftrl/lambda1>
    .lambda2                  <Ftrl/lambda2>
    .model                    <Ftrl/model>
    .model_type               <Ftrl/model_type>
    .model_type_trained       <Ftrl/model_type_trained>
    .mantissa_nbits           <Ftrl/mantissa_nbits>
    .nbins                    <Ftrl/nbins>
    .negative_class           <Ftrl/negative_class>
    .nepochs                  <Ftrl/nepochs>
    .params                   <Ftrl/params>
    .predict()                <Ftrl/predict>
    .reset()                  <Ftrl/reset>

