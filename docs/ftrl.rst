FTRL
====

This section describes the FTRL (Follow the Regularized Leader) model as implemented in datatable.

FTRL Model Information
----------------------

The Follow the Regularized Leader (FTRL) model is a datatable implementation of
the FTRL-Proximal online learning
`algorithm <https://research.google.com/pubs/archive/41159.pdf>`__
for binomial logistic regression. It uses a
`hashing trick <https://en.wikipedia.org/wiki/Feature_hashing>`__
for feature vectorization and the
`Hogwild approach
<https://people.eecs.berkeley.edu/~brecht/papers/hogwildTR.pdf>`__
for parallelization. FTRL for multinomial classification and continuous
targets are implemented experimentally.

Create an FTRL Model
--------------------

The FTRL model is implemented as the ``Ftrl`` Python class, which is a part of
``datatable.models``, so to use the model you should first do

::

  from datatable.models import Ftrl

and then create a model as

::

  ftrl_model = Ftrl()


FTRL Model Parameters
---------------------

The FTRL model requires a list of parameters for training and making predictions,
namely:

-  ``alpha`` – learning rate, defaults to ``0.005``.
-  ``beta`` – beta parameter, defaults to ``1.0``.
-  ``lambda1`` – L1 regularization parameter, defaults to ``0.0``.
-  ``lambda2`` – L2 regularization parameter, defaults to ``1.0``.
-  ``nbins`` – the number of bins for the hashing trick, defaults to ``10**6``.
-  ``float_nbits`` – the number of bits to be taken into account when hashing float values,
  defaults to maximum of ``sizeof(double) * 8``, that is ``64`` on most of the systems.
-  ``nepochs`` – the number of epochs to train the model for, defaults to ``1``.
-  ``negative_class`` – whether to create and train on a "negative" class in the case of multinomial classification, defaults to ``False``.

If some parameters need to be changed, this can be done either
when creating the model, as

::

  ftrl_model = Ftrl(alpha = 0.1, nbins = 100)

or, if the model already exists, as

::

  ftrl_model.alpha = 0.1
  ftrl_model.nbins = 100

If some parameters were not set explicitely, they will be assigned the default
values.


Training a Model
----------------

Use the ``fit()`` method to train a model:

::

  ftrl_model.fit(X_train, y_train)

where ``X_train`` is a frame of shape ``(nrows, ncols)`` to be trained on,
and ``y_train`` is a target frame of shape ``(nrows, 1)``. The following
datatable column types are supported for the ``X_train`` frame: ``bool``,
``int``, ``real`` and ``str``.


FTRL model can also do early stopping, if relative validation error does
not improve. For this the model should be fit as

::

  res = ftrl_model.fit(X_train, y_train, X_validation, y_validation, nepochs_validation, validation_error)


where ``X_train`` and ``y_train`` are training and target frames,
respectively, ``X_validation`` and ``y_validation`` are validation frames,
``nepochs_validation`` specifies how often, in epoch units, validation
error should be checked, and ``validation_error`` is the relative
validation error improvement that the model should demonstrate within
``nepochs_validation`` to continue training. Returned ``res`` tuple
contains epoch at which training stopped and the corresponding loss.


Resetting a Model
-----------------

Use the ``reset()`` method to reset a model:

::

  ftrl_model.reset()

This will reset model weights, but it will not affect learning parameters.
To reset parameters to default values, you can do

::

  ftrl_model.params = Ftrl().params


Making Predictions
------------------

Use the ``predict()`` method to make predictions:

::

  targets = ftrl_model.predict(X)

where ``X`` is a frame of shape ``(nrows, ncols)`` to make predictions for.
``X`` should have the same number of columns as the training frame.
The ``predict()`` method returns a new frame of shape ``(nrows, 1)`` with
the predicted probability for each row of frame ``X``.


Feature Importances
-------------------

To estimate feature importances, the overall weight contributions are
calculated feature-wise during training and predicting. Feature importances
can be accessed as

::

  fi = ftrl_model.feature_importances

where ``fi`` will be a frame of shape ``(nfeatures, 2)`` containing
feature names and their importances, that are normalized to [0; 1] range.


Feature Interactions
--------------------

By default each column of a training dataset is considered as a feature
by FTRL model. User can provide additional features by specifying
a list of feature interactions, for instance as

::

  ftrl_model.interactions = [["C0", "C1", "C3"], ["C2", "C5"]]

where ``C*`` are column names from a training dataset. In the above example
two additional features, namely, ``C0:C1:C3`` and ``C2:C5``, are created.

``interactions`` should be set before a call to ``fit()`` method, and can not be
changed once the model is trained.


Further Reading
---------------

For detailed help, please also refer to ``help(Ftrl)``.
