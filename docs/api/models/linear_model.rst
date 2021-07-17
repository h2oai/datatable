
.. xclass:: datatable.models.LinearModel
    :src: src/core/models/py_linearmodel.h LinearModel
    :tests: tests/models/test-linearmodel.py
    :cvar: doc_models_LM

    This class implements the `Linear model`_ with the
    `stochastic gradient descent`_ learning. It supports linear
    regression, as well as binomial and multinomial classification.
    Both :meth:`.fit` and :meth:`.predict` methods are fully parallel.

    .. _`Linear model`: https://en.wikipedia.org/wiki/Linear_model
    .. _`stochastic gradient descent`: https://en.wikipedia.org/wiki/Stochastic_gradient_descent


    Construction
    ------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`LinearModel() <datatable.models.LinearModel.__init__>`
          - Construct a `LinearModel` object.



    Methods
    -------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`fit() <datatable.models.LinearModel.fit>`
          - Train model on the input samples and targets.

        * - :meth:`is_fitted() <datatable.models.LinearModel.is_fitted>`
          - Report model status.

        * - :meth:`predict() <datatable.models.LinearModel.predict>`
          - Predict for the input samples.

        * - :meth:`reset() <datatable.models.LinearModel.reset>`
          - Reset the model.


    Properties
    ----------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :attr:`eta0 <datatable.models.LinearModel.eta0>`
          - Initial learning rate.

        * - :attr:`eta_decay <datatable.models.LinearModel.eta_decay>`
          - Decay for the `"time-based"` and `"step-based"` learning rate schedules.

        * - :attr:`eta_drop_rate <datatable.models.LinearModel.eta_drop_rate>`
          - Drop rate for the `"step-based"` learning rate schedule.

        * - :attr:`eta_schedule <datatable.models.LinearModel.eta_schedule>`
          - Learning rate schedule.

        * - :attr:`double_precision <datatable.models.LinearModel.double_precision>`
          - An option to control precision of the internal computations.

        * - :attr:`labels <datatable.models.LinearModel.labels>`
          - Classification labels.

        * - :attr:`lambda1 <datatable.models.LinearModel.lambda2>`
          - L1 regularization parameter.

        * - :attr:`lambda2 <datatable.models.LinearModel.lambda2>`
          - L2 regularization parameter.

        * - :attr:`model <datatable.models.LinearModel.model>`
          - Model coefficients.

        * - :attr:`model_type <datatable.models.LinearModel.model_type>`
          - Model type to be built.

        * - :attr:`negative_class <datatable.models.LinearModel.negative_class>`
          - An option to indicate if the "negative" class should be a created
            for multinomial classification.

        * - :attr:`nepochs <datatable.models.LinearModel.nepochs>`
          - Number of training epochs.

        * - :attr:`params <datatable.models.LinearModel.params>`
          - All the input model parameters as a named tuple.

        * - :attr:`seed <datatable.models.LinearModel.seed>`
          - Seed for the quasi-random data shuffling.


.. toctree::
    :hidden:

    .__init__()               <LinearModel/__init__>
    .eta0                     <LinearModel/eta0>
    .eta_decay                <LinearModel/eta_decay>
    .eta_drop_rate            <LinearModel/eta_drop_rate>
    .eta_schedule             <LinearModel/eta_schedule>
    .double_precision         <LinearModel/double_precision>
    .fit()                    <LinearModel/fit>
    .is_fitted()              <LinearModel/is_fitted>
    .labels                   <LinearModel/labels>
    .lambda1                  <LinearModel/lambda1>
    .lambda2                  <LinearModel/lambda2>
    .model                    <LinearModel/model>
    .model_type               <LinearModel/model_type>
    .negative_class           <LinearModel/negative_class>
    .nepochs                  <LinearModel/nepochs>
    .params                   <LinearModel/params>
    .predict()                <LinearModel/predict>
    .reset()                  <LinearModel/reset>
    .seed                     <LinearModel/seed>

