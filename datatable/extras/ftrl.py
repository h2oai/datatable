#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable.lib import core


def ftrl(dt_train, dt_test, a=0.01, b=1.0, l1=0.0, l2=1.0, 
         d=10**7, n_epochs=1, inter=False, hash_type=2):
    """
    Implementation of Follow the Regularized Leader (FTRL) algorithm.
    
    For more details see this reference:
    https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf

    Parameters
    ----------
    dt_train: datatable
        Frame to be trained on, last column is treated as `target`.
    dt_test: datatable
        Frame to be tested on, must have one column less than `dt_train`.
    a : float
        \alpha in per-coordinate learning rate formula.
    b : float
        \beta in per-coordinate learning rate formula.
    l1 : float
        L1 regularization parameter.
    l2 : float
        L2 regularization parameter.
    d : int
        Number of bins to be used after the hashing trick. 
    n_epochs : int
        Number of epochs to train on.
    inter : boolean
        If feature interactions to be used or not.
    hash_type : int
        Hashing method to use:
          `0` - leave numeric values as they are;
          `1` - convert all values to strings and std::hash them 
                including the column names.
          `2` - use MurmurHash3 to hash string values and combine this
                with the numeric values.

    Returns
    -------
     A new datatable of shape (nrows, 1) containing target values for each
     row from `dt_test`.
    """
    
    df_target = core.ftrl(dt_train, dt_test, a, b, l1, l2, 
                          d, n_epochs, inter, hash_type)
    return df_target
