
.. xclass:: datatable.models.Ftrl
    :src: src/core/models/py_ftrl.h Ftrl
    :doc: src/core/models/py_ftrl.cc doc_Ftrl
    :tests: tests/models/test-ftrl.py


    Construction
    ------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`Ftrl() <datatable.models.Ftrl.__init__>`
          - Construct the `Ftrl` object.



    Methods
    -------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`fit() <datatable.models.Ftrl.fit>`
          - Train the model.

        * - :meth:`predict() <datatable.models.Ftrl.predict>`
          - Predict for a trained model.

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

        * - :attr:`lambda1 <datatable.models.Ftrl.lambda1>`
          - L1 regularization parameter.

        * - :attr:`lambda2 <datatable.models.Ftrl.lambda2>`
          - L2 regularization parameter.

        * - :attr:`mantissa_nbits <datatable.models.Ftrl.mantissa_nbits>`
          - Number of mantissa bits for hashing floats.

        * - :attr:`model <datatable.models.Ftrl.model>`
          - The built model coefficients.

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

