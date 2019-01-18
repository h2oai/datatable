FTRL
====

This section describes FTRL model as implemented in DataTable.

FTRL Model Information
----------------------

Follow the Regularized Leader (FTRL) model is a DataTable implementation of 
FTRL-Proximal online learning 
`algorithm <https://research.google.com/pubs/archive/41159.pdf>`__
for binomial logistic regression. It uses a
`hashing trick <https://en.wikipedia.org/wiki/Feature_hashing>`__
for feature vectorization and
`Hogwild approach 
<https://people.eecs.berkeley.edu/~brecht/papers/hogwildTR.pdf>`__
for parallelization. FTRL for m classification and continuous 
targets are implemented experimentally.

Create FTRL Model
-----------------

FTRL model is implemented as an ``Ftrl`` Python class, a part of
``datatable.models``, so to use the model one should first do

::

  from datatable.models import Ftrl

and then create a model as

::

  ftrl_model = Ftrl()
  
  
FTRL Model Parameters
---------------------

FTRL model requires a list of parameters for training and making predictions,
namely:

-  ``alpha`` — learning rate;
-  ``beta`` — beta parameter;
-  ``lambda1`` — L1 regularization parameter;
-  ``lambda2`` — L2 regularization parameter;
-  ``d`` number — of bins for the hashing trick;
-  ``nepochs`` — number of epochs to train the model for;
-  ``inter`` — switch to enable second order feature interactions.

If no parameters are passed to ``Ftrl`` constructor, 
the following default parameters will be used to create a model

::

  >>> Ftrl().params
  FtrlParams(alpha=0.005, beta=1.0, lambda1=0.0, lambda2=1.0, d=1000000, n_epochs=1, inter=False)

If some parameters need to be changed, this can be done either
when creating the model

::

  ftrl_model = Ftrl(alpha = 0.1, d = 100, inter = False)
  
or, if the model already exists, as

::

  ftrl_model.alpha = 0.1
  ftrl_model.d = 100
  ftrl_model.inter = False

If some parameters were not set explicitely, they will be assigned default
values.


Training a Model
----------------

To train a model for binomial logistic regression problem, ``fit()`` method should be
used

::

  ftrl_model.fit(X, y)
  
where ``X`` is a frame of shape ``(nrows, ncols)`` to be trained on,
and ``y`` is a frame of shape ``(nrows, 1)`` having a ``bool`` type
of the target column.


Reseting a Model
----------------

To reset a model ``reset()`` method should be used as

::

  ftrl_model.reset()

This will reset model weights, but will not affect learning parameters.
To reset parameters to default values one can do

::

  ftrl_model.params = Ftrl().params
  

Making Predictions
------------------

To make predictions ``predict()`` method should be used

::

  targets = ftrl_model.predict(X)
  
where ``X`` is a frame of shape ``(nrows, ncols)`` to make predictions for.
It should have the same number of columns as the training frame had.
``predict()`` method returns a new frame of shape ``(nrows, 1)`` with
the predicted probability for each row of frame ``X``.


Feature Importances
-------------------

To estimate feature importances, the overall weight contributions are
calculated feature-wise during training and predicting. Feature importances
can be accessed as

::

  ftrl_model.feature_importances
  
that returns a frame of shape ``(nfeatures, 1)`` containing
importance information, that is normalized to [0; 1] range.


Further Reading
---------------

For detailed help please also refer to ``help(Ftrl)``.
