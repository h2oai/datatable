#!/usr/bin/env python
# -*- coding: utf-8 -*-
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
#
# Test FTRL modeling capabilities
#
#-------------------------------------------------------------------------------
import datatable as dt
from datatable.lib import core
import pytest
import collections
import random
from tests import assert_equals
from tests import noop

#-------------------------------------------------------------------------------
# Define namedtuple of test parameters
#-------------------------------------------------------------------------------
Params = collections.namedtuple("Params",["alpha", "beta", "lambda1", "lambda2",
                                          "d", "n_epochs", "inter"])
test_params = Params(alpha = 1, beta = 2, lambda1 = 3, lambda2 = 4, d = 5,
                     n_epochs = 6, inter = True)


#-------------------------------------------------------------------------------
# Test wrong parameter types, names and combination in constructor
#-------------------------------------------------------------------------------
 
def test_ftrl_construct_wrong_alpha():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(alpha = "1.0"))
    assert ("Argument `alpha` in Ftrl() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_beta():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(beta = "1.0"))
    assert ("Argument `beta` in Ftrl() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_lambda1():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(lambda1 = "1.0"))
    assert ("Argument `lambda1` in Ftrl() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_lambda2():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(lambda2 = "1.0"))
    assert ("Argument `lambda2` in Ftrl() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_d():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(d = 1000000.0))
    assert ("Argument `d` in Ftrl() constructor should be an integer, instead "
            "got <class 'float'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_n_epochs():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(n_epochs = 10.0))
    assert ("Argument `n_epochs` in Ftrl() constructor should be an integer, instead "
            "got <class 'float'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_inter():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(inter = 2))
    assert ("Argument `inter` in Ftrl() constructor should be a boolean, instead "
            "got <class 'int'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_combination():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(params=test_params, alpha = test_params.alpha))
    assert ("You can either pass all the parameters with `params` or  any of "
            "the individual parameters with `alpha`, `beta`, `lambda1`, "
            "`lambda2`, `d`,`n_epchs` or `inter` to Ftrl constructor, "
            "but not both at the same time" ==
            str(e.value))
 
 
def test_ftrl_construct_unknown_arg():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(c = 1.0))
    assert ("Ftrl() constructor got an unexpected keyword argument `c`" ==
            str(e.value))


#-------------------------------------------------------------------------------
# Test creation of Ftrl object
#-------------------------------------------------------------------------------

def test_ftrl_create_default():
    ft = core.Ftrl()
    assert ft.params == ft.default_params


def test_ftrl_create_params():
    ft = core.Ftrl(params=test_params)
    assert ft.params == test_params


def test_ftrl_create_individual():
    ft = core.Ftrl(alpha = test_params.alpha, beta = test_params.beta,
                   lambda1 = test_params.lambda1, lambda2 = test_params.lambda2,
                   d = test_params.d, n_epochs = test_params.n_epochs,
                   inter = test_params.inter)
    assert ft.params == (test_params.alpha, test_params.beta,
                         test_params.lambda1, test_params.lambda2,
                         test_params.d, test_params.n_epochs, test_params.inter)


#-------------------------------------------------------------------------------
# Test getters, setters and reset methods for FTRL parameters
#-------------------------------------------------------------------------------

def test_ftrl_get_params():
    ft = core.Ftrl(params = test_params)
    assert ft.params == test_params
    assert (ft.alpha, ft.beta, ft.lambda1, ft.lambda2,
            ft.d, ft.n_epochs, ft.inter) == test_params


def test_ftrl_set_individual():
    ft = core.Ftrl()
    ft.alpha = test_params.alpha
    ft.beta = test_params.beta
    ft.lambda1 = test_params.lambda1
    ft.lambda2 = test_params.lambda2
    ft.d = test_params.d
    ft.n_epochs = test_params.n_epochs
    ft.inter = test_params.inter
    assert ft.params == test_params


def test_ftrl_set_wrong_alpha():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.alpha = "1.0"
    assert ("Expected a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_beta():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.beta = "1.0"
    assert ("Expected a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_lambda1():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.lambda1 = "1.0"
    assert ("Expected a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_lambda2():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.lambda2 = "1.0"
    assert ("Expected a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_d():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.d = 1000000.0
    assert ("Expected an integer, instead got <class 'float'>" == str(e.value))


def test_ftrl_set_wrong_n_epochs():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.n_epochs = 10.0
    assert ("Expected an integer, instead got <class 'float'>" == str(e.value))


def test_ftrl_set_wrong_inter():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.inter = 2
    assert ("Expected a boolean, instead got <class 'int'>" == str(e.value))


def test_ftrl_set_wrong_params_type():
    ft = core.Ftrl()
    params = test_params._replace(alpha = "1.0")
    with pytest.raises(TypeError) as e:
        ft.params = params
    assert ("Expected a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_params_name():
    ft = core.Ftrl()
    WrongParams = collections.namedtuple("WrongParams",["alpha", "inter"])
    wrong_params = WrongParams(alpha = 1, inter = True)
    with pytest.raises(AttributeError) as e:
        ft.params = wrong_params
    assert ("'WrongParams' object has no attribute 'beta'" == str(e.value))


def test_ftrl_set_params():
    ft = core.Ftrl()
    ft.params = test_params
    assert ft.params == test_params


def test_ftrl_reset_params():
    ft = core.Ftrl(params = test_params)
    assert ft.params == test_params
    ft.reset_params()
    assert ft.params == ft.default_params


#-------------------------------------------------------------------------------
# Test getters, setters and reset methods for FTRL model
#-------------------------------------------------------------------------------

def test_ftrl_model_none():
    ft = core.Ftrl()
    assert ft.model == None


def test_ftrl_get_set_reset_model():
    ft = core.Ftrl(params = test_params)
    model = dt.Frame([[random.random() for i in range(test_params.d)],
                     [random.random() for i in range(test_params.d)]],
                     names=['z', 'n'])
    ft.model = model
    assert_equals(ft.model, model)
    ft.reset_model()
    assert ft.model == None


#-------------------------------------------------------------------------------
# Test wrong training frame
#-------------------------------------------------------------------------------

def test_ftrl_fit_wrong_empty():
    ft = core.Ftrl()
    df_train = dt.Frame()
    with pytest.raises(ValueError) as e:
        ft.fit(df_train)
    assert ("Cannot train a model on an empty frame" ==
            str(e.value))


def test_ftrl_fit_wrong_one_column():
    ft = core.Ftrl()
    df_train = dt.Frame([1, 2, 3])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train)
    assert ("Training frame must have at least two columns" ==
            str(e.value))


def test_ftrl_fit_wrong_target_integer():
    ft = core.Ftrl()
    df_train = dt.Frame([[1, 2, 3], [4, 5, 6]])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train)
    assert ("Last column in a training frame must have a `bool` type" ==
            str(e.value))


def test_ftrl_fit_wrong_target_real():
    ft = core.Ftrl()
    df_train = dt.Frame([[1, 2, 3], [4.0, 5.0, 6.0]])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train)
    assert ("Last column in a training frame must have a `bool` type" ==
            str(e.value))


def test_ftrl_fit_wrong_target_string():
    ft = core.Ftrl()
    df_train = dt.Frame([[1, 2, 3], ["Monday", "Tuesday", "Friday"]])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train)
    assert ("Last column in a training frame must have a `bool` type" ==
            str(e.value))


#-------------------------------------------------------------------------------
# Test hash function
#-------------------------------------------------------------------------------

def test_ftrl_col_hashes():
    ncols = 10
    col_hashes_murmur2 = ( 1838936504594058908, 14027412581578625840,
                          14296604503264754754,  3956937694466614811,
                          10071734010655191393,  6063711047550005084,
                           4309007444360962581,  4517980897659475069,
                          17871586791652695964, 15779814813469047786)
    ft = core.Ftrl()
    df_train = dt.Frame([[0]] * ncols + [[True]])
    ft.fit(df_train)
    assert col_hashes_murmur2 == ft.colnames_hashes


#-------------------------------------------------------------------------------
# Test wrong prediction frame
#-------------------------------------------------------------------------------

def test_ftrl_predict_not_trained():
    ft = core.Ftrl()
    df_train = dt.Frame([[1, 2, 3], [True, False, True]])
    with pytest.raises(ValueError) as e:
        ft.predict(df_train)
    assert ("Cannot make any predictions, because the model was not trained"
            == str(e.value))


def test_ftrl_predict_wrong_columns():
    ft = core.Ftrl()
    df_train = dt.Frame([[1, 2, 3], [True, False, True]])
    ft.fit(df_train)
    with pytest.raises(ValueError) as e:
        ft.predict(df_train)
    assert ("Can only predict on a frame that has %d column(s), i.e. has the "
            "same number of features as was used for model training"
            % (df_train.ncols - 1) == str(e.value))


#-------------------------------------------------------------------------------
# Test training `fit` and `predict` methods
#-------------------------------------------------------------------------------

epsilon = 0.01

def test_ftrl_fit_unique():
    ft = core.Ftrl(d = 10)
    df_train = dt.Frame([[i for i in range(ft.d)],
                         [True for i in range(ft.d)]])
    ft.fit(df_train)
    model = [[-0.5 for i in range(ft.d)], [0.25 for i in range(ft.d)]]
    assert ft.model.topython() == model


def test_ftrl_fit_predict_bool():
    ft = core.Ftrl(alpha = 0.1, n_epochs = 10000)
    df_train = dt.Frame([[True, False],
                         [True, False]])
    ft.fit(df_train)
    df_target = ft.predict(df_train[:,0])
    assert df_target[0, 0] <= 1
    assert df_target[0, 0] >= 1 - epsilon
    assert df_target[1, 0] >= 0
    assert df_target[1, 0] < epsilon


def test_ftrl_fit_predict_int():
    ft = core.Ftrl(alpha = 0.1, n_epochs = 10000)
    df_train = dt.Frame([[0, 1],
                         [True, False]])
    ft.fit(df_train)
    df_target = ft.predict(df_train[:,0])
    assert df_target[0, 0] <= 1
    assert df_target[0, 0] >= 1 - epsilon
    assert df_target[1, 0] >= 0
    assert df_target[1, 0] < epsilon


def test_ftrl_fit_predict_float():
    ft = core.Ftrl(alpha = 0.1, n_epochs = 10000)
    df_train = dt.Frame([[0.0, 0.1],
                         [True, False]])
    ft.fit(df_train)
    df_target = ft.predict(df_train[:,0])
    assert df_target[0, 0] <= 1
    assert df_target[0, 0] >= 1 - epsilon
    assert df_target[1, 0] >= 0
    assert df_target[1, 0] < epsilon


def test_ftrl_fit_predict_string():
    ft = core.Ftrl(alpha = 0.1, n_epochs = 10000)
    df_train = dt.Frame([["Monday", "Tuesday"],
                         [True, False]])
    ft.fit(df_train)
    df_target = ft.predict(df_train[:,0])
    assert df_target[0, 0] <= 1
    assert df_target[0, 0] >= 1 - epsilon
    assert df_target[1, 0] >= 0
    assert df_target[1, 0] < epsilon
