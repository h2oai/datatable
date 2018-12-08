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
from datatable import f
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
tparams = Params(alpha = 1, beta = 2, lambda1 = 3, lambda2 = 4, d = 5,
                     n_epochs = 6, inter = True)

tmodel = dt.Frame([[random.random() for i in range(tparams.d)],
                      [random.random() for i in range(tparams.d)]],
                      names=['z', 'n'])

default_params = Params(alpha = 0.005, beta = 1, lambda1 = 0, lambda2 = 1,
                        d = 1000000, n_epochs = 1, inter = False)

#-------------------------------------------------------------------------------
# Test wrong parameter types, names and combination in constructor
#-------------------------------------------------------------------------------
 
def test_ftrl_construct_wrong_alpha_type():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(alpha = "1.0"))
    assert ("Argument `alpha` in Ftrl() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_beta_type():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(beta = "1.0"))
    assert ("Argument `beta` in Ftrl() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_lambda1_type():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(lambda1 = "1.0"))
    assert ("Argument `lambda1` in Ftrl() constructor should be a float, "
            "instead got <class 'str'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_lambda2_type():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(lambda2 = "1.0"))
    assert ("Argument `lambda2` in Ftrl() constructor should be a float, "
            "instead got <class 'str'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_d_type():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(d = 1000000.0))
    assert ("Argument `d` in Ftrl() constructor should be an integer, instead "
            "got <class 'float'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_n_epochs_type():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(n_epochs = 10.0))
    assert ("Argument `n_epochs` in Ftrl() constructor should be an integer, "
            "instead got <class 'float'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_inter_type():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(inter = 2))
    assert ("Argument `inter` in Ftrl() constructor should be a boolean, "
            "instead got <class 'int'>" == str(e.value))
 
 
def test_ftrl_construct_wrong_combination():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(params=tparams, alpha = tparams.alpha))
    assert ("You can either pass all the parameters with `params` or any of "
            "the individual parameters with `alpha`, `beta`, `lambda1`, "
            "`lambda2`, `d`, `n_epochs` or `inter` to Ftrl constructor, "
            "but not both at the same time" == str(e.value))
 
 
def test_ftrl_construct_unknown_arg():
    with pytest.raises(TypeError) as e:
        noop(core.Ftrl(c = 1.0))
    assert ("Ftrl() constructor got an unexpected keyword argument `c`" ==
            str(e.value))


#-------------------------------------------------------------------------------
# Test wrong parameter values in constructor
#-------------------------------------------------------------------------------

def test_ftrl_construct_wrong_alpha_value():
    with pytest.raises(ValueError) as e:
        noop(core.Ftrl(alpha = 0.0))
    assert ("Argument `alpha` in Ftrl() constructor should be positive: 0.0" 
            == str(e.value))
 
 
def test_ftrl_construct_wrong_beta_value():
    with pytest.raises(ValueError) as e:
        noop(core.Ftrl(beta = -1.0))
    assert ("Argument `beta` in Ftrl() constructor cannot be negative: -1.0"
            == str(e.value))
 
 
def test_ftrl_construct_wrong_lambda1_value():
    with pytest.raises(ValueError) as e:
        noop(core.Ftrl(lambda1 = -1.0))
    assert ("Argument `lambda1` in Ftrl() constructor cannot be negative: -1.0"
            == str(e.value))
 
 
def test_ftrl_construct_wrong_lambda2_value():
    with pytest.raises(ValueError) as e:
        noop(core.Ftrl(lambda2 = -1.0))
    assert ("Argument `lambda2` in Ftrl() constructor cannot be negative: -1.0"
            == str(e.value))
 
 
def test_ftrl_construct_wrong_d_value():
    with pytest.raises(ValueError) as e:
        noop(core.Ftrl(d = 0))
    assert ("Argument `d` in Ftrl() constructor should be positive: 0"
            == str(e.value))
 
 
def test_ftrl_construct_wrong_n_epochs_value():
    with pytest.raises(ValueError) as e:
        noop(core.Ftrl(n_epochs = -1))
    assert ("Argument `n_epochs` in Ftrl() constructor cannot be negative: -1"
            == str(e.value))

    
#-------------------------------------------------------------------------------
# Test creation of Ftrl object
#-------------------------------------------------------------------------------

def test_ftrl_create_default():
    ft = core.Ftrl()
    assert ft.params == default_params


def test_ftrl_create_params():
    ft = core.Ftrl(tparams)
    assert ft.params == tparams


def test_ftrl_create_individual():
    ft = core.Ftrl(alpha = tparams.alpha, beta = tparams.beta,
                   lambda1 = tparams.lambda1, lambda2 = tparams.lambda2,
                   d = tparams.d, n_epochs = tparams.n_epochs,
                   inter = tparams.inter)
    assert ft.params == (tparams.alpha, tparams.beta,
                         tparams.lambda1, tparams.lambda2,
                         tparams.d, tparams.n_epochs, tparams.inter)


#-------------------------------------------------------------------------------
# Test getters and setters for valid FTRL parameters
#-------------------------------------------------------------------------------

def test_ftrl_get_parameters():
    ft = core.Ftrl(tparams)
    assert ft.params == tparams
    assert (ft.alpha, ft.beta, ft.lambda1, ft.lambda2,
            ft.d, ft.n_epochs, ft.inter) == tparams


def test_ftrl_set_individual():
    ft = core.Ftrl()
    ft.alpha = tparams.alpha
    ft.beta = tparams.beta
    ft.lambda1 = tparams.lambda1
    ft.lambda2 = tparams.lambda2
    ft.d = tparams.d
    ft.n_epochs = tparams.n_epochs
    ft.inter = tparams.inter
    assert ft.params == tparams


def test_ftrl_set_params():
    ft = core.Ftrl()
    ft.params = tparams
    assert ft.params == tparams


#-------------------------------------------------------------------------------
# Test getters and setters for wrong types / names of FTRL parameters
#-------------------------------------------------------------------------------

def test_ftrl_set_wrong_params_type_type():
    ft = core.Ftrl()
    params = tparams._replace(alpha = "1.0")
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


def test_ftrl_set_wrong_alpha_type():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.alpha = "0.0"
    assert ("Expected a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_beta_type():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.beta = "-1.0"
    assert ("Expected a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_lambda1_type():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.lambda1 = "-1.0"
    assert ("Expected a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_lambda2_type():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.lambda2 = "-1.0"
    assert ("Expected a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_d_type():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.d = "0"
    assert ("Expected an integer, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_n_epochs_type():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.n_epochs = "-10.0"
    assert ("Expected an integer, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_inter_type():
    ft = core.Ftrl()
    with pytest.raises(TypeError) as e:
        ft.inter = 2
    assert ("Expected a boolean, instead got <class 'int'>" == str(e.value))


#-------------------------------------------------------------------------------
# Test getters and setters for wrong values of individual FTRL parameters
#-------------------------------------------------------------------------------

def test_ftrl_set_wrong_alpha_value():
    ft = core.Ftrl()
    with pytest.raises(ValueError) as e:
        ft.alpha = 0.0
    assert ("Float value is not positive" == str(e.value))


def test_ftrl_set_wrong_beta_value():
    ft = core.Ftrl()
    with pytest.raises(ValueError) as e:
        ft.beta = -1.0
    assert ("Float value is negative" == str(e.value))


def test_ftrl_set_wrong_lambda1_value():
    ft = core.Ftrl()
    with pytest.raises(ValueError) as e:
        ft.lambda1 = -1.0
    assert ("Float value is negative" == str(e.value))


def test_ftrl_set_wrong_lambda2_value():
    ft = core.Ftrl()
    with pytest.raises(ValueError) as e:
        ft.lambda2 = -1.0
    assert ("Float value is negative" == str(e.value))


def test_ftrl_set_wrong_d_value():
    ft = core.Ftrl()
    with pytest.raises(ValueError) as e:
        ft.d = 0
    assert ("Integer value is not positive" == str(e.value))


def test_ftrl_set_wrong_n_epochs_value():
    ft = core.Ftrl()
    with pytest.raises(ValueError) as e:
        ft.n_epochs = -10
    assert ("Integer value is negative" == str(e.value))

#-------------------------------------------------------------------------------
# Test getters, setters and reset methods for FTRL model
#-------------------------------------------------------------------------------

def test_ftrl_model_untrained():
    ft = core.Ftrl()
    assert ft.model == None
    

def test_ftrl_set_negative_n_model():
    ft = core.Ftrl(tparams)
    with pytest.raises(ValueError) as e:
        ft.model = tmodel[:, {'z' : f.z, 'n' : -f.n}]
    assert ("Values in column `n` cannot be negative" == str(e.value))


def test_ftrl_set_wrong_shape_model():
    ft = core.Ftrl(tparams)
    with pytest.raises(ValueError) as e:
        ft.model = tmodel[:, 'n']
    assert ("FTRL model frame must have %d rows, and 2 columns, whereas your "
            "frame has %d rows and 1 column(s)" % (tparams.d, tparams.d)
            == str(e.value))


def test_ftrl_set_wrong_type_model():
    ft = core.Ftrl(tparams)
    model = dt.Frame([["foo" for i in range(tparams.d)],
                      [random.random() for i in range(tparams.d)]],
                      names=['z', 'n'])
    with pytest.raises(ValueError) as e:
        ft.model = model
    assert ("FTRL model frame must have both column types as `float64`, whereas"
            " your frame has the following column types: `str32` and `float64`"
            == str(e.value)) 


def test_ftrl_get_set_model():
    ft = core.Ftrl(tparams)
    ft.model = tmodel
    assert_equals(ft.model, tmodel)


def test_ftrl_reset_model():
    ft = core.Ftrl(tparams)
    ft.model = tmodel
    ft.reset()
    assert ft.model == None

    
def test_ftrl_none_model():
    ft = core.Ftrl(tparams)
    ft.model = None
    assert ft.model == None


#-------------------------------------------------------------------------------
# Test wrong training frame
#-------------------------------------------------------------------------------

def test_ftrl_fit_wrong_empty_training():
    ft = core.Ftrl()
    df_train = dt.Frame()
    df_target = dt.Frame([True])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train, df_target)
    assert ("Training frame must have at least one column" ==
            str(e.value))


def test_ftrl_fit_wrong_empty_target():
    ft = core.Ftrl()
    df_train = dt.Frame([1.0, 2.0])
    df_target = dt.Frame()
    with pytest.raises(ValueError) as e:
        ft.fit(df_train, df_target)
    assert ("Target frame must have exactly one column" ==
            str(e.value))


def test_ftrl_fit_wrong_target_integer():
    ft = core.Ftrl()
    df_train = dt.Frame([1, 2, 3])
    df_target = dt.Frame([4, 5, 6])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train, df_target)
    assert ("Target column must be of a `bool` type" ==
            str(e.value))


def test_ftrl_fit_wrong_target_real():
    ft = core.Ftrl()
    df_train = dt.Frame([1, 2, 3])
    df_target = dt.Frame([4.0, 5.0, 6.0])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train, df_target)
    assert ("Target column must be of a `bool` type" ==
            str(e.value))


def test_ftrl_fit_wrong_target_string():
    ft = core.Ftrl()
    df_train = dt.Frame([1, 2, 3])
    df_target = dt.Frame(["Monday", "Tuesday", "Wedenesday"])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train, df_target)
    assert ("Target column must be of a `bool` type" ==
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
    df_train = dt.Frame([[0]] * ncols)
    df_target = dt.Frame([[True]])
    ft.fit(df_train, df_target)
    assert col_hashes_murmur2 == ft.colnames_hashes


#-------------------------------------------------------------------------------
# Test wrong parameters for `fit` and `predict` methods
#-------------------------------------------------------------------------------


def test_ftrl_fit_no_frame():
    ft = core.Ftrl()
    with pytest.raises(ValueError) as e:
        ft.fit()
    assert ("Training frame parameter is missing"
            == str(e.value))
    
    
def test_ftrl_fit_no_target():
    ft = core.Ftrl()
    with pytest.raises(ValueError) as e:
        ft.fit(None)
    assert ("Target frame parameter is missing"
            == str(e.value))
    
    
def test_ftrl_fit_predict_nones():
    ft = core.Ftrl()
    ft.fit(None, None)
    df_target = ft.predict(None)
    assert df_target == None
    

def test_ftrl_predict_not_trained():
    ft = core.Ftrl()
    df_train = dt.Frame([[1, 2, 3], [True, False, True]])
    with pytest.raises(ValueError) as e:
        ft.predict(df_train)
    assert ("Cannot make any predictions, because the model was not trained"
            == str(e.value))


def test_ftrl_predict_wrong_frame():
    ft = core.Ftrl()
    df_train = dt.Frame([[1, 2, 3]])
    df_target = dt.Frame([[True, False, True]])
    df_predict = dt.Frame([[1, 2, 3], [4, 5, 6]])
    ft.fit(df_train, df_target)
    with pytest.raises(ValueError) as e:
        ft.predict(df_predict)
    assert ("Can only predict on a frame that has %d column(s), i.e. has the "
            "same number of features as was used for model training"
            % (df_train.ncols) == str(e.value))


#-------------------------------------------------------------------------------
# Test `fit` and `predict` methods
#-------------------------------------------------------------------------------

epsilon = 0.01

def test_ftrl_fit_unique():
    ft = core.Ftrl(d = 10)
    df_train = dt.Frame([[i for i in range(ft.d)]])
    df_target = dt.Frame([[True for i in range(ft.d)]])
    ft.fit(df_train, df_target)
    model = [[-0.5 for i in range(ft.d)], [0.25 for i in range(ft.d)]]
    assert ft.model.topython() == model


def test_ftrl_fit_predict_bool():
    ft = core.Ftrl(alpha = 0.1, n_epochs = 10000)
    df_train = dt.Frame([[True, False]])
    df_target = dt.Frame([[True, False]])
    ft.fit(df_train, df_target)
    df_target = ft.predict(df_train[:,0])
    assert df_target[0, 0] <= 1
    assert df_target[0, 0] >= 1 - epsilon
    assert df_target[1, 0] >= 0
    assert df_target[1, 0] < epsilon


def test_ftrl_fit_predict_int():
    ft = core.Ftrl(alpha = 0.1, n_epochs = 10000)
    df_train = dt.Frame([[0, 1]])
    df_target = dt.Frame([[True, False]])
    ft.fit(df_train, df_target)
    df_target = ft.predict(df_train[:,0])
    assert df_target[0, 0] <= 1
    assert df_target[0, 0] >= 1 - epsilon
    assert df_target[1, 0] >= 0
    assert df_target[1, 0] < epsilon


def test_ftrl_fit_predict_float():
    ft = core.Ftrl(alpha = 0.1, n_epochs = 10000)
    df_train = dt.Frame([[0.0, 1.0]])
    df_target = dt.Frame([[True, False]])
    ft.fit(df_train, df_target)
    df_target = ft.predict(df_train[:,0])
    assert df_target[0, 0] <= 1
    assert df_target[0, 0] >= 1 - epsilon
    assert df_target[1, 0] >= 0
    assert df_target[1, 0] < epsilon


def test_ftrl_fit_predict_string():
    ft = core.Ftrl(alpha = 0.1, n_epochs = 10000)
    df_train = dt.Frame([["Monday", "Tuesday"]])
    df_target = dt.Frame([[True, False]])
    ft.fit(df_train, df_target)
    df_target = ft.predict(df_train[:,0])
    assert df_target[0, 0] <= 1
    assert df_target[0, 0] >= 1 - epsilon
    assert df_target[1, 0] >= 0
    assert df_target[1, 0] < epsilon
