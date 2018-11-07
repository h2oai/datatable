#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable.lib import core


def ftrl(df_train, df_test, a=0.01, b=1.0, l1=0.0, l2=1.0, 
         d=10**7, n_epochs=1, inter=False, hash_type=1,
         seed=0):
    """
    Implementation of Follow the Regularized Leader (FTRL) algorithm.
    
    For more details see this reference:
    https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf

    Parameters
    ----------
    df_train: datatable
        Frame to be trained on, last column is treated as `target`.
    df_test: datatable
        Frame to be tested on, must have one column less than `df_train`.
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
        Number of epochs to train for.
    inter : boolean
        If feature interactions to be used or not.
    hash_type : int
        Hashing method to use for strings:
          `0` - std::hash;
          `1` - Murmur2;
          `2` - Murmur3.
    seed: unsigned int
        Seed to be used for initialization and Murmur hash functions.

    Returns
    -------
     A new datatable of shape (nrows, 1) containing target values for each
     row from `df_test`.
    """
    
    df_target = core.ftrl(df_train, df_test, a, b, l1, l2, 
                          d, n_epochs, inter, hash_type, seed)
    return df_target
