FTRL
====

This section describes FTRL model implemented in DataTable.

FTRL information
----------------

Follow the Regularized Leader (FTRL) is a DataTable implementation of 
FTRL-Proximal online learning `algorithm <https://research.google.com/pubs/archive/41159.pdf>`__
for binomial logistic regression. This implementation uses a hashing trick and 
`Hogwild approach <https://people.eecs.berkeley.edu/~brecht/papers/hogwildTR.pdf>`__
for parallelization. Multinomial logistic regression and regression for continuous 
targets are implemented in an experimental mode.

Create FTRL model
-----------------

To create FTRL model with default parameters simply do

::

  import datatable as dt
  from datatable import models
  
  ftrl_model = Ftrl()


