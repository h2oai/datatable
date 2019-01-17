FTRL
====

This section describes FTRL model as implemented in DataTable.

FTRL model information
----------------------

Follow the Regularized Leader (FTRL) model is a DataTable implementation of 
FTRL-Proximal online learning `algorithm <https://research.google.com/pubs/archive/41159.pdf>`__
for binomial logistic regression. It uses a `hashing trick <https://en.wikipedia.org/wiki/Feature_hashing>`__
for feature vectorization and `Hogwild approach <https://people.eecs.berkeley.edu/~brecht/papers/hogwildTR.pdf>`__
for parallelization. Multinomial logistic regression and regression for continuous 
targets are implemented experimentally.

Create FTRL model
-----------------

FTRL model is implemented as an ``Ftrl`` Python class, a part of ``datatable.models``,
so to use the model one should first do

::

  from datatable.models import Ftrl

and then create a model as easy as 

::

  ftrl_model = Ftrl()
  
  
FTRL model parameters
---------------------

FTRL model requires a list of parameters for training and making predictions,
namely:

-  ``alpha``: learning rate;
-  ``beta``: beta parameter;
-  ``lambda1``: L1 regularization parameter;
-  ``lambda2``: L2 regularization parameter;
-  ``d``: number of bins for the hashing trick;
-  ``nepochs``: number of epochs to train the model for;
-  ``inter``: switch to enable second order feature interactions.

If no parameters are passed to ``Ftrl`` constructor, 
default parameters will be used for the model. A list
of default parameters can be accessed as 

::

  Ftrl().params

A list of the actual model parameters can be accessed in a similar way

::

  ftrl_model.params 

Parameters can be also accessed individualy as

::

  ftrl_model.alpha
  ftrl_model.beta
  ftrl_model.lambda1
  ...
  

FTRL parameters can be passed to the model either during initialization

::

  ftrl_model = Ftrl(alpha = 0.1, d = 100, inter = False)
  
or be set individually as

::

  ftrl_model.alpha = 0.1
  ftrl_model.d = 100
  ftrl_model.inter = False


Training a model
----------------

For binomial logistic regression training a model is
as easy as

::

  ftrl_model.fit(X, y)
  
where ``X`` is a frame of shape ``(nrows, ncols)`` to be trained on,
and ``y`` is a frame of shape ``(nrows, 1)``, i.e. a target column that has 
a ``bool`` type.

Making predictions
------------------

When the model is trained, one can make predictions as

::

  targets = ftrl_model.predict(X)
  
where ``X`` is a frame of shape ``(nrows, ncols)`` to make predictions for.
It should have the same number of columns as the training frames had. ``predict()``
method returns a new frame of shape ``(nrows, 1)`` with the predicted probability
for each row of frame ``X``.