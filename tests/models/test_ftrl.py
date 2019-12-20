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
import pickle
import pytest
import collections
import random
import math
import datatable as dt
from datatable.models import Ftrl
from datatable.internal import frame_integrity_check
from datatable import f, stype, DatatableWarning
from tests import assert_equals, noop


#-------------------------------------------------------------------------------
# Define namedtuple of test parameters, test model and accuracy
#-------------------------------------------------------------------------------
Params = collections.namedtuple("FtrlParams",["alpha", "beta", "lambda1", "lambda2",
                                          "nbins", "mantissa_nbits", "nepochs",
                                          "double_precision", "negative_class",
                                          "interactions", "model_type"])
tparams = Params(alpha = 1, beta = 2, lambda1 = 3, lambda2 = 4, nbins = 5,
                 mantissa_nbits = 6, nepochs = 7, double_precision = True,
                 negative_class = True, interactions = (("C0",),),
                 model_type = 'binomial')

tmodel = dt.Frame([[random.random() for _ in range(tparams.nbins)],
                   [random.random() for _ in range(tparams.nbins)]],
                   names=['z', 'n'])

default_params = Params(alpha = 0.005, beta = 1, lambda1 = 0, lambda2 = 0,
                        nbins = 10**6, mantissa_nbits = 10, nepochs = 1,
                        double_precision = False, negative_class = False,
                        interactions = None, model_type = 'auto')
epsilon = 0.01


#-------------------------------------------------------------------------------
# Test wrong parameter types, names and combination in constructor
#-------------------------------------------------------------------------------

def test_ftrl_construct_wrong_alpha_type():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(alpha = "1.0"))
    assert ("Argument `alpha` in Ftrl() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))


def test_ftrl_construct_wrong_beta_type():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(beta = "1.0"))
    assert ("Argument `beta` in Ftrl() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))


def test_ftrl_construct_wrong_lambda1_type():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(lambda1 = "1.0"))
    assert ("Argument `lambda1` in Ftrl() constructor should be a float, "
            "instead got <class 'str'>" == str(e.value))


def test_ftrl_construct_wrong_lambda2_type():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(lambda2 = "1.0"))
    assert ("Argument `lambda2` in Ftrl() constructor should be a float, "
            "instead got <class 'str'>" == str(e.value))


def test_ftrl_construct_wrong_nbins_type():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(nbins = 1000000.0))
    assert ("Argument `nbins` in Ftrl() constructor should be an integer, "
            "instead got <class 'float'>" == str(e.value))


def test_ftrl_construct_wrong_mantissa_nbits_type():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(mantissa_nbits = 20.0))
    assert ("Argument `mantissa_nbits` in Ftrl() constructor should be an integer, "
            "instead got <class 'float'>" == str(e.value))


def test_ftrl_construct_wrong_nepochs_type():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(nepochs = 10.0))
    assert ("Argument `nepochs` in Ftrl() constructor should be an integer, "
            "instead got <class 'float'>" == str(e.value))


def test_ftrl_construct_wrong_double_precision_type():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(double_precision = 2))
    assert ("Argument `double_precision` in Ftrl() constructor should be a boolean, "
            "instead got <class 'int'>" == str(e.value))


def test_ftrl_construct_wrong_interactions_type():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(interactions = "C0"))
    assert ("Argument `interactions` in Ftrl() constructor should be a list or a tuple, "
            "instead got: <class 'str'>" == str(e.value))


def test_ftrl_construct_wrong_interaction_type():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(interactions = ["C0"]))
    assert ("Argument `interactions` in Ftrl() constructor should be a list or a tuple "
            "of lists or tuples, instead encountered: 'C0'" == str(e.value))


def test_ftrl_construct_wrong_interactions_from_itertools():
    import itertools
    with pytest.raises(TypeError) as e:
        noop(Ftrl(interactions = itertools.combinations(["C0", "C1"], 2)))
    assert ("Argument `interactions` in Ftrl() constructor should be a list or a tuple, "
            "instead got: <class 'itertools.combinations'>"
            == str(e.value))


def test_ftrl_construct_wrong_combination():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(params=tparams, alpha = tparams.alpha))
    assert ("You can either pass all the parameters with `params` or any of "
            "the individual parameters with `alpha`, `beta`, `lambda1`, "
            "`lambda2`, `nbins`, `mantissa_nbits`, `nepochs`, `double_precision`, "
            "`negative_class`, `interactions` or `model_type` to Ftrl constructor, "
            "but not both at the same time"
            == str(e.value))


def test_ftrl_construct_unknown_arg():
    with pytest.raises(TypeError) as e:
        noop(Ftrl(c = 1.0))
    assert ("Ftrl() constructor got an unexpected keyword argument `c`" ==
            str(e.value))


def test_ftrl_construct_wrong_params_type():
    params = tparams._replace(alpha = "1.0")
    with pytest.raises(TypeError) as e:
        Ftrl(params)
    assert ("`FtrlParams.alpha` should be a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_construct_wrong_params_name():
    WrongParams = collections.namedtuple("WrongParams",["alpha", "lambda1"])
    wrong_params = WrongParams(alpha = 1, lambda1 = 0.01)
    with pytest.raises(AttributeError) as e:
        Ftrl(wrong_params)
    assert ("'WrongParams' object has no attribute 'double_precision'"
            == str(e.value))


#-------------------------------------------------------------------------------
# Test wrong parameter values in constructor
#-------------------------------------------------------------------------------

def test_ftrl_construct_wrong_alpha_value():
    with pytest.raises(ValueError) as e:
        noop(Ftrl(alpha = 0.0))
    assert ("Argument `alpha` in Ftrl() constructor should be positive: 0.0"
            == str(e.value))


def test_ftrl_construct_wrong_beta_value():
    with pytest.raises(ValueError) as e:
        noop(Ftrl(beta = -1.0))
    assert ("Argument `beta` in Ftrl() constructor should be greater than "
            "or equal to zero: -1.0" == str(e.value))


def test_ftrl_construct_wrong_lambda1_value():
    with pytest.raises(ValueError) as e:
        noop(Ftrl(lambda1 = -1.0))
    assert ("Argument `lambda1` in Ftrl() constructor should be greater than "
            "or equal to zero: -1.0" == str(e.value))


def test_ftrl_construct_wrong_lambda2_value():
    with pytest.raises(ValueError) as e:
        noop(Ftrl(lambda2 = -1.0))
    assert ("Argument `lambda2` in Ftrl() constructor should be greater than "
            "or equal to zero: -1.0" == str(e.value))


def test_ftrl_construct_wrong_nbins_value():
    with pytest.raises(ValueError) as e:
        noop(Ftrl(nbins = 0))
    assert ("Argument `nbins` in Ftrl() constructor should be positive: 0"
            == str(e.value))


@pytest.mark.parametrize('value, message',
                         [[-1, "Argument `mantissa_nbits` in Ftrl() constructor cannot be negative: -1"],
                         [53, "Argument `mantissa_nbits` in Ftrl() constructor should be less than or equal to 52, got: 53"]])
def test_ftrl_construct_wrong_mantissa_nbits_value(value, message):
    with pytest.raises(ValueError) as e:
        noop(Ftrl(mantissa_nbits = value))
    assert (message == str(e.value))


def test_ftrl_construct_wrong_nepochs_value():
    with pytest.raises(ValueError) as e:
        noop(Ftrl(nepochs = -1))
    assert ("Argument `nepochs` in Ftrl() constructor cannot be negative: -1"
            == str(e.value))


#-------------------------------------------------------------------------------
# Test creation of Ftrl object
#-------------------------------------------------------------------------------

def test_ftrl_create_default():
    ft = Ftrl()
    assert ft.params == default_params
    assert ft.model_type_trained == "none"
    assert ft.labels == None


def test_ftrl_access_params():
    import sys
    ft = Ftrl()
    tuple_refcount = sys.getrefcount(tuple)
    namedtuple_refcount_old = sys.getrefcount(type(ft.params))
    for _ in range(tuple_refcount + 1):
        assert ft.params == default_params
    namedtuple_refcount_new = sys.getrefcount(type(ft.params))
    assert namedtuple_refcount_new == namedtuple_refcount_old


def test_ftrl_create_params():
    ft = Ftrl(tparams)
    assert ft.params == tparams


def test_ftrl_create_individual():
    ft = Ftrl(alpha = tparams.alpha, beta = tparams.beta,
              lambda1 = tparams.lambda1, lambda2 = tparams.lambda2,
              nbins = tparams.nbins, mantissa_nbits = tparams.mantissa_nbits,
              nepochs = tparams.nepochs,
              double_precision = tparams.double_precision,
              negative_class = tparams.negative_class,
              interactions = tparams.interactions,
              model_type = tparams.model_type)
    assert ft.params == (tparams.alpha, tparams.beta,
                         tparams.lambda1, tparams.lambda2,
                         tparams.nbins, tparams.mantissa_nbits, tparams.nepochs,
                         tparams.double_precision, tparams.negative_class,
                         tparams.interactions, tparams.model_type)


#-------------------------------------------------------------------------------
# Test getters and setters for valid FTRL parameters
#-------------------------------------------------------------------------------

def test_ftrl_get_params():
    ft = Ftrl(tparams)
    params = ft.params
    assert params == tparams
    assert (ft.alpha, ft.beta, ft.lambda1, ft.lambda2, ft.nbins, ft.mantissa_nbits,
           ft.nepochs, ft.double_precision, ft.negative_class, ft.interactions,
           ft.model_type) == tparams
    assert (params.alpha, params.beta, params.lambda1, params.lambda2, params.nbins,
           params.mantissa_nbits, params.nepochs, params.double_precision,
           params.negative_class, params.interactions, params.model_type) == tparams


def test_ftrl_set_individual():
    ft = Ftrl(double_precision = tparams.double_precision)
    ft.alpha = tparams.alpha
    ft.beta = tparams.beta
    ft.lambda1 = tparams.lambda1
    ft.lambda2 = tparams.lambda2
    ft.nbins = tparams.nbins
    ft.mantissa_nbits = tparams.mantissa_nbits
    ft.nepochs = tparams.nepochs
    ft.negative_class = tparams.negative_class
    ft.interactions = tparams.interactions
    ft.model_type = tparams.model_type


def test_ftrl_set_individual_after_params():
    ft = Ftrl()
    params = ft.params
    ft.alpha = tparams.alpha
    params_new = ft.params
    assert params == Ftrl().params
    assert (params_new.alpha, params_new.beta, params_new.lambda1, params_new.lambda2,
           params_new.nbins, params_new.mantissa_nbits, params_new.nepochs,
           params_new.double_precision, params_new.negative_class,
           params_new.interactions, params_new.model_type) == params_new
    assert (ft.alpha, ft.beta, ft.lambda1, ft.lambda2,
           ft.nbins, ft.mantissa_nbits, ft.nepochs,
           ft.double_precision, ft.negative_class,
           ft.interactions, ft.model_type) == params_new

#-------------------------------------------------------------------------------
# Test getters and setters for wrong types / names of FTRL parameters
#-------------------------------------------------------------------------------

def test_ftrl_set_wrong_alpha_type():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.alpha = "0.0"
    assert ("`.alpha` should be a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_beta_type():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.beta = "-1.0"
    assert ("`.beta` should be a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_lambda1_type():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.lambda1 = "-1.0"
    assert ("`.lambda1` should be a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_lambda2_type():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.lambda2 = "-1.0"
    assert ("`.lambda2` should be a float, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_nbins_type():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.nbins = "0"
    assert ("`.nbins` should be an integer, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_mantissa_nbits_type():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.mantissa_nbits = "0"
    assert ("`.mantissa_nbits` should be an integer, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_nepochs_type():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.nepochs = "-10.0"
    assert ("`.nepochs` should be an integer, instead got <class 'str'>" == str(e.value))


def test_ftrl_set_wrong_interactions_type():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.interactions = True
    assert ("`.interactions` should be a list or a tuple, instead got: <class 'bool'>"
            == str(e.value))


def test_ftrl_set_wrong_interactions_empty():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.interactions = [["C0"], []]
    assert ("Interaction cannot have zero features, encountered: []" == str(e.value))


def test_ftrl_set_wrong_interactions_not_list():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.interactions = ["a", [1, 2]]
    assert ("`.interactions` should be a list or a tuple of lists or tuples, "
           "instead encountered: 'a'" == str(e.value))


def test_ftrl_set_wrong_interactions_not_string():
    ft = Ftrl()
    with pytest.raises(TypeError) as e:
        ft.interactions = [["a", "b"], [1, 2]]
    assert ("Interaction features should be strings, instead "
            "encountered: 1" == str(e.value))


#-------------------------------------------------------------------------------
# Test getters and setters for wrong values of individual FTRL parameters
#-------------------------------------------------------------------------------

@pytest.mark.parametrize('value, message',
                         [[0.0, "`.alpha` should be positive: 0.0"],
                          [None, "`.alpha` should be positive: None"],
                          [math.nan, "`.alpha` should be positive: nan"],
                          [math.inf, "`.alpha` should be finite: inf"]
                         ])
def test_ftrl_set_bad_alpha_value(value, message):
    ft = Ftrl()
    with pytest.raises(ValueError) as e:
        ft.alpha = value
    assert (message == str(e.value))


@pytest.mark.parametrize('value, message',
                         [[-1.0, "`.beta` should be greater than or equal to zero: -1.0"],
                          [None, "`.beta` should be greater than or equal to zero: None"],
                          [math.nan, "`.beta` should be greater than or equal to zero: nan"],
                          [math.inf, "`.beta` should be finite: inf"]
                         ])
def test_ftrl_set_bad_beta_value(value, message):
    ft = Ftrl()
    with pytest.raises(ValueError) as e:
        ft.beta = value
    assert (message == str(e.value))


@pytest.mark.parametrize('value, message',
                         [[-1.0, "`.lambda1` should be greater than or equal to zero: -1.0"],
                          [None, "`.lambda1` should be greater than or equal to zero: None"],
                          [math.nan, "`.lambda1` should be greater than or equal to zero: nan"],
                          [math.inf, "`.lambda1` should be finite: inf"]
                         ])
def test_ftrl_set_bad_lambda1_value(value, message):
    ft = Ftrl()
    with pytest.raises(ValueError) as e:
        ft.lambda1 = value
    assert (message == str(e.value))


@pytest.mark.parametrize('value, message',
                         [[-1.0, "`.lambda2` should be greater than or equal to zero: -1.0"],
                          [None, "`.lambda2` should be greater than or equal to zero: None"],
                          [math.nan, "`.lambda2` should be greater than or equal to zero: nan"],
                          [math.inf, "`.lambda2` should be finite: inf"]
                         ])
def test_ftrl_set_bad_lambda2_value(value, message):
    ft = Ftrl()
    with pytest.raises(ValueError) as e:
        ft.lambda2 = value
    assert (message == str(e.value))


def test_ftrl_set_wrong_nbins_value():
    ft = Ftrl()
    with pytest.raises(ValueError) as e:
        ft.nbins = 0
    assert ("`.nbins` should be positive: 0" == str(e.value))


@pytest.mark.parametrize('value, message',
                         [[-1, "`.mantissa_nbits` cannot be negative: -1"],
                         [53, "`.mantissa_nbits` should be less than or equal to 52, got: 53"]])
def test_ftrl_set_wrong_mantissa_nbits_value(value, message):
    ft = Ftrl()
    with pytest.raises(ValueError) as e:
        ft.mantissa_nbits = value
    assert (message == str(e.value))


def test_ftrl_set_wrong_nepochs_value():
    ft = Ftrl()
    with pytest.raises(ValueError) as e:
        ft.nepochs = -10
    assert ("`.nepochs` cannot be negative: -10" == str(e.value))


#-------------------------------------------------------------------------------
# Test wrong training frame
#-------------------------------------------------------------------------------

def test_ftrl_fit_wrong_empty_training():
    ft = Ftrl()
    df_train = dt.Frame()
    df_target = dt.Frame([True])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train, df_target)
    assert ("Training frame must have at least one column" ==
            str(e.value))


def test_ftrl_fit_wrong_empty_target():
    ft = Ftrl()
    df_train = dt.Frame([1.0, 2.0])
    df_target = dt.Frame()
    with pytest.raises(ValueError) as e:
        ft.fit(df_train, df_target)
    assert ("Target frame must have exactly one column" ==
            str(e.value))


def test_ftrl_fit_wrong_target_obj64():
    ft = Ftrl()
    df_train = dt.Frame(list(range(8)))
    df_target = dt.Frame([3, "point", None, None, 14, 15, 92, "6"])
    with pytest.raises(TypeError) as e:
        ft.fit(df_train, df_target)
    assert ("Target column type `obj64` is not supported" ==
            str(e.value))


#-------------------------------------------------------------------------------
# Test hash function
#-------------------------------------------------------------------------------

def test_ftrl_colname_hashes():
    ncols = 10
    col_hashes_murmur2 = [ 1838936504594058908, 14027412581578625840,
                          14296604503264754754,  3956937694466614811,
                          10071734010655191393,  6063711047550005084,
                           4309007444360962581,  4517980897659475069,
                          17871586791652695964, 15779814813469047786]
    ft = Ftrl()
    df_train = dt.Frame([[0]] * ncols)
    df_target = dt.Frame([[True]])
    ft.fit(df_train, df_target)
    assert col_hashes_murmur2 == ft.colname_hashes


#-------------------------------------------------------------------------------
# Test wrong parameters for `fit` and `predict` methods
#-------------------------------------------------------------------------------

def test_ftrl_model_untrained():
    ft = Ftrl()
    assert ft.model == None


def test_ftrl_fit_no_frame():
    ft = Ftrl()
    with pytest.raises(ValueError) as e:
        ft.fit()
    assert ("Training frame parameter is missing"
            == str(e.value))


def test_ftrl_fit_no_target():
    ft = Ftrl()
    with pytest.raises(ValueError) as e:
        ft.fit(None)
    assert ("Target frame parameter is missing"
            == str(e.value))


def test_ftrl_fit_predict_nones():
    ft = Ftrl()
    ft.fit(None, None)
    df_target = ft.predict(None)
    assert df_target == None


def test_ftrl_predict_not_trained():
    ft = Ftrl()
    df_train = dt.Frame([[1, 2, 3], [True, False, True]])
    with pytest.raises(ValueError) as e:
        ft.predict(df_train)
    assert ("Cannot make any predictions, the model should be trained first"
            == str(e.value))


def test_ftrl_predict_wrong_frame():
    ft = Ftrl()
    df_train = dt.Frame([[1, 2, 3]])
    df_target = dt.Frame([[True, False, True]])
    df_predict = dt.Frame([[1, 2, 3], [4, 5, 6]])
    ft.fit(df_train, df_target)
    with pytest.raises(ValueError) as e:
        ft.predict(df_predict)
    assert ("Can only predict on a frame that has 1 column, i.e. has the "
            "same number of features as was used for model training"
            == str(e.value))


#-------------------------------------------------------------------------------
# Test reset
#-------------------------------------------------------------------------------

def test_ftrl_reset_untrained():
    ft = Ftrl(tparams)
    ft.reset()
    assert ft.params == tparams


def test_ftrl_reset_trained():
    ft = Ftrl(tparams)
    ft.fit(dt.Frame(range(10)), dt.Frame([False, True] * 5))
    model_ref = dt.Frame([[0.0] * tparams.nbins, [7.0] * tparams.nbins])
    assert_equals(ft.model, model_ref)
    ft.reset()
    assert ft.model == None
    assert ft.params == tparams
    ft.nepochs = 0
    ft.fit(dt.Frame(range(10)), dt.Frame([False, True] * 5))
    assert_equals(ft.model, dt.Frame([[0.0] * tparams.nbins] * 2))



#-------------------------------------------------------------------------------
# Test `fit` and `predict` methods for binomial classification
#-------------------------------------------------------------------------------

def test_ftrl_fit_none():
    ft = Ftrl(nbins = 10, model_type = "binomial")
    df_train = dt.Frame(range(ft.nbins))
    df_target = dt.Frame([None] * ft.nbins)
    res = ft.fit(df_train, df_target)
    assert not ft.labels
    assert ft.model_type == "binomial"
    assert ft.model_type_trained == "none"
    assert res.epoch == 0.0
    assert res.loss is None


def test_ftrl_fit_unique():
    ft = Ftrl(nbins = 10)
    df_train = dt.Frame(range(ft.nbins))
    df_target = dt.Frame([True] * ft.nbins)
    ft.fit(df_train, df_target)
    model = [[0.5] * ft.nbins, [0.25] * ft.nbins]
    assert_equals(
        ft.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert ft.model_type_trained == "binomial"
    assert ft.model.to_list() == model


def test_ftrl_fit_unique_ignore_none():
    ft = Ftrl(nbins = 10)
    df_train = dt.Frame(range(2 * ft.nbins))
    df_target = dt.Frame([True] * ft.nbins + [None] * ft.nbins)
    ft.fit(df_train, df_target)
    model = [[0.5] * ft.nbins, [0.25] * ft.nbins]
    assert_equals(
        ft.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert ft.model_type_trained == "binomial"
    assert ft.model.to_list() == model


def test_ftrl_fit_predict_bool():
    ft = Ftrl(alpha = 0.1, nepochs = 10000)
    df_train = dt.Frame([[True, False]])
    df_target = dt.Frame([[True, False]])
    ft.fit(df_train, df_target)
    df_target = ft.predict(df_train[:,0])
    assert_equals(
        ft.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert ft.model_type_trained == "binomial"
    assert df_target[0, 1] <= 1
    assert df_target[0, 1] >= 1 - epsilon
    assert df_target[1, 1] >= 0
    assert df_target[1, 1] < epsilon


def test_ftrl_fit_predict_int():
    ft = Ftrl(alpha = 0.1, nepochs = 10000)
    df_train = dt.Frame([[0, 1]])
    df_target = dt.Frame([[True, False]])
    ft.fit(df_train, df_target)
    df_target = ft.predict(df_train[:,0])
    assert ft.model_type_trained == "binomial"
    assert df_target[0, 1] <= 1
    assert df_target[0, 1] >= 1 - epsilon
    assert df_target[1, 1] >= 0
    assert df_target[1, 1] < epsilon


def test_ftrl_fit_predict_float():
    ft = Ftrl(alpha = 0.1, nepochs = 10000)
    df_train = dt.Frame([[0.0, 1.0, math.inf]])
    df_target = dt.Frame([[True, False, False]])
    ft.fit(df_train, df_target)
    df_target = ft.predict(df_train[:,0])
    assert ft.model_type_trained == "binomial"
    assert df_target[0, 1] <= 1
    assert df_target[0, 1] >= 1 - epsilon
    assert df_target[1, 1] >= 0
    assert df_target[1, 1] < epsilon


def test_ftrl_fit_predict_string():
    ft = Ftrl(alpha = 0.1, nepochs = 10000)
    df_train = dt.Frame([["Monday", None, "", "Tuesday"]])
    df_target = dt.Frame([[True, False, False, True]])
    ft.fit(df_train, df_target)
    df_target = ft.predict(df_train[:,0])
    assert ft.model_type_trained == "binomial"
    assert df_target[0, 1] <= 1
    assert df_target[0, 1] >= 1 - epsilon
    assert df_target[1, 1] >= 0
    assert df_target[1, 1] < epsilon


@pytest.mark.parametrize('target',
                         [[True, False],
                         ["yes", "no"],
                         [20, 10],
                         [0.5, -0.5]])
def test_ftrl_fit_predict_bool_binomial(target):
    ft = Ftrl(alpha = 0.1, nepochs = 10000, model_type = "binomial")
    df_train = dt.Frame([True, False])
    df_target = dt.Frame(target)
    ft.fit(df_train, df_target)
    df_res = ft.predict(df_train)
    target.sort()
    ids = [1, 0]
    # When a target column has a boolean type, negatives
    # will always be assigned a `0` label id.
    if ft.labels.stypes[0] == dt.stype.bool8:
        ids.sort()
    assert_equals(
        ft.labels,
        dt.Frame(label=target, id=ids, stypes={"id" : stype.int32})
    )
    assert ft.labels[:, 0].to_list() == [sorted(target)]
    assert ft.model_type_trained == "binomial"
    assert df_res[0, 1] <= 1
    assert df_res[0, 1] >= 1 - epsilon
    assert df_res[1, 1] >= 0
    assert df_res[1, 1] < epsilon
    assert df_res[0, 0] >= 0
    assert df_res[0, 0] < epsilon
    assert df_res[1, 0] <= 1
    assert df_res[1, 0] >= 1 - epsilon


def test_ftrl_fit_predict_view():
    ft = Ftrl(nbins = 100)
    # Generate unique numbers, so that this test can be run in parallel
    df_train = dt.Frame(random.sample(range(ft.nbins), ft.nbins))
    df_target = dt.Frame([bool(random.getrandbits(1)) for _ in range(ft.nbins)])
    rows = range(ft.nbins//2, ft.nbins)

    # Train model and predict on a view
    ft.fit(df_train[rows,:], df_target[rows,:])
    predictions = ft.predict(df_train[rows,:])
    model = ft.model

    # Train model and predict on a frame
    ft.reset()
    df_train_range = df_train[rows,:]
    df_target_range = df_target[rows,:]
    df_train_range.materialize()
    df_target_range.materialize()
    ft.fit(df_train_range, df_target_range)
    predictions_range = ft.predict(df_train_range)

    assert_equals(
        ft.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert ft.model_type_trained == "binomial"
    assert_equals(model, ft.model)
    assert_equals(predictions, predictions_range)


@pytest.mark.parametrize('parameter, value',
                         [("nbins", 100),
                         ("interactions", [["C0", "C0"]]),
                         ("mantissa_nbits", 46),
                         ("negative_class", True)])
def test_ftrl_disable_setters_after_fit(parameter, value):
    ft = Ftrl(nbins = 10)
    df_train = dt.Frame(range(ft.nbins))
    df_target = dt.Frame([True] * ft.nbins)
    ft.fit(df_train, df_target)

    assert ft.model_type_trained == "binomial"
    with pytest.raises(ValueError) as e:
        setattr(ft, parameter, value)
    assert ("Cannot change `."+parameter+"` for a trained model, "
            "reset this model or create a new one"
            == str(e.value))
    ft.reset()
    setattr(ft, parameter, value)


def test_ftrl_fit_predict_binomial_online_1_1():
    ft = Ftrl(alpha = 0.1, nepochs = 10000, model_type = "binomial")
    df_train_odd = dt.Frame([[1, 3, 7, 5, 9]])
    df_target_odd = dt.Frame([["odd", "odd", "odd", "odd", "odd"]])
    ft.fit(df_train_odd, df_target_odd)
    assert_equals(
        ft.labels,
        dt.Frame(label=["odd"], id=[0], stypes={"id": dt.int32}
    )
                  )

    df_train_even = dt.Frame([[2, 4, 8, 6]])
    df_target_even = dt.Frame([["even", "even", "even", "even"]])
    ft.fit(df_train_even, df_target_even)
    assert_equals(
        ft.labels,
        dt.Frame(label=["even", "odd"], id=[1, 0], stypes={"id": dt.int32})
    )

    df_train_wrong = dt.Frame([[2, 4, None, 6]])
    df_target_wrong = dt.Frame([["even", "even", "none", "even"]])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train_wrong, df_target_wrong)
    assert ("Got a new label in the target column, however, both positive and "
            "negative labels are already set"
            == str(e.value))

    p = ft.predict(df_train_odd)
    p_dict = p.to_dict()
    delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [1, 1, 1, 1, 1])]
    delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [0, 0, 0, 0, 0])]
    assert ft.model_type_trained == "binomial"
    assert max(delta_odd) < epsilon
    assert max(delta_even) < epsilon

    p = ft.predict(df_train_even)
    p_dict = p.to_dict()
    delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 1, 1, 1])]
    delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 0, 0, 0])]
    assert ft.model_type_trained == "binomial"
    assert max(delta_odd) < epsilon
    assert max(delta_even) < epsilon


def test_ftrl_fit_predict_binomial_online_1_2():
    ft = Ftrl(alpha = 0.1, nepochs = 10000, model_type = "binomial")
    df_train_odd = dt.Frame([[1, 3, 7, 5, 9]])
    df_target_odd = dt.Frame([["odd", "odd", "odd", "odd", "odd"]])
    ft.fit(df_train_odd, df_target_odd)
    assert_equals(
        ft.labels,
        dt.Frame(label=["odd"], id=[0], stypes={"id": dt.int32})
    )

    df_train_wrong = dt.Frame([[2, 4, None, 6]])
    df_target_wrong = dt.Frame([["even", "even", "none", "even"]])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train_wrong, df_target_wrong)
    assert ("Got two new labels in the target column, however, positive "
            "label is already set"
            == str(e.value))

    df_train_even_odd = dt.Frame([[2, 1, 8, 3]])
    df_target_even_odd = dt.Frame([["even", "odd", "even", "odd"]])
    ft.fit(df_train_even_odd, df_target_even_odd)
    assert_equals(
        ft.labels,
        dt.Frame(label=["even", "odd"], id=[1, 0], stypes={"id": dt.int32})
    )

    p = ft.predict(df_train_odd)
    p_dict = p.to_dict()
    delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [1, 1, 1, 1, 1])]
    delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [0, 0, 0, 0, 0])]
    assert ft.model_type_trained == "binomial"
    assert max(delta_odd) < epsilon
    assert max(delta_even) < epsilon

    p = ft.predict(df_train_even_odd)
    p_dict = p.to_dict()
    delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 0, 1, 0])]
    delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 1, 0, 1])]
    assert ft.model_type_trained == "binomial"
    assert max(delta_odd) < epsilon
    assert max(delta_even) < epsilon


def test_ftrl_fit_predict_binomial_online_2_1():
    ft = Ftrl(alpha = 0.1, nepochs = 10000, model_type = "binomial")
    df_train_even_odd = dt.Frame([[2, 1, 8, 3]])
    df_target_even_odd = dt.Frame([["even", "odd", "even", "odd"]])
    ft.fit(df_train_even_odd, df_target_even_odd)
    assert_equals(
        ft.labels,
        dt.Frame(label=["even", "odd"], id=[0, 1], stypes={"id": dt.int32})
    )

    df_train_odd = dt.Frame([[1, 3, 7, 5, 9]])
    df_target_odd = dt.Frame([["odd", "odd", "odd", "odd", "odd"]])
    ft.fit(df_train_odd, df_target_odd)
    assert_equals(
        ft.labels,
        dt.Frame(label=["even", "odd"], id=[0, 1], stypes={"id": dt.int32})
    )

    df_train_wrong = dt.Frame([[math.inf, math.inf, None, math.inf]])
    df_target_wrong = dt.Frame([["inf", "inf", "none", "inf"]])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train_wrong, df_target_wrong)
    assert ("Got a new label in the target column, however, both positive and "
            "negative labels are already set"
            == str(e.value))


    p = ft.predict(df_train_odd)
    p_dict = p.to_dict()
    delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [1, 1, 1, 1, 1])]
    delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [0, 0, 0, 0, 0])]
    assert ft.model_type_trained == "binomial"
    assert max(delta_odd) < epsilon
    assert max(delta_even) < epsilon

    p = ft.predict(df_train_even_odd)
    p_dict = p.to_dict()
    delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 0, 1, 0])]
    delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 1, 0, 1])]
    assert ft.model_type_trained == "binomial"
    assert max(delta_odd) < epsilon
    assert max(delta_even) < epsilon


def test_ftrl_fit_predict_binomial_online_2_2():
    ft = Ftrl(alpha = 0.1, nepochs = 10000, model_type = "binomial")
    df_train_even_odd = dt.Frame([[2, 1, 8, 3]])
    df_target_even_odd = dt.Frame([["even", "odd", "even", "odd"]])
    ft.fit(df_train_even_odd, df_target_even_odd)
    assert_equals(
        ft.labels,
        dt.Frame(label=["even", "odd"], id=[0, 1], stypes={"id": dt.int32})
    )

    df_train_odd_even = dt.Frame([[1, 2, 3, 8]])
    df_target_odd_even = dt.Frame([["odd", "even", "odd", "even"]])
    ft.fit(df_train_odd_even, df_target_odd_even)
    assert_equals(
        ft.labels,
        dt.Frame(label=["even", "odd"], id=[0, 1], stypes={"id": dt.int32})
    )

    df_train_wrong = dt.Frame([[math.inf, math.inf, None, math.inf]])
    df_target_wrong = dt.Frame([["inf", "inf", "none", "inf"]])
    with pytest.raises(ValueError) as e:
        ft.fit(df_train_wrong, df_target_wrong)
    assert ("Got a new label in the target column, however, both positive and "
            "negative labels are already set"
            == str(e.value))

    p = ft.predict(df_train_even_odd)
    p_dict = p.to_dict()
    delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 0, 1, 0])]
    delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 1, 0, 1])]
    assert ft.model_type_trained == "binomial"
    assert max(delta_odd) < epsilon
    assert max(delta_even) < epsilon


#-------------------------------------------------------------------------------
# Test multinomial regression
#-------------------------------------------------------------------------------

def test_ftrl_fit_multinomial_none():
    ft = Ftrl(nbins = 10, model_type = "multinomial")
    df_train = dt.Frame(range(ft.nbins))
    df_target = dt.Frame([None] * ft.nbins)
    res = ft.fit(df_train, df_target)
    assert not ft.labels
    assert ft.model_type == "multinomial"
    assert ft.model_type_trained == "none"
    assert res.epoch == 0.0
    assert res.loss is None


def test_ftrl_fit_predict_multinomial_vs_binomial():
    target_names = ["cat", "dog"]
    ft_binomial = Ftrl(nbins = 10, nepochs = 1)
    df_train_binomial = dt.Frame(range(10))
    df_target_binomial = dt.Frame({"target" : [False, True] * 5})
    ft_binomial.fit(df_train_binomial, df_target_binomial)
    p_binomial = ft_binomial.predict(df_train_binomial)
    p_binomial.names = target_names

    ft_multinomial = Ftrl(nbins = 10, nepochs = 1,
                          model_type = "multinomial")
    df_target_multinomial = dt.Frame(target_names * 5)
    ft_multinomial.fit(df_train_binomial, df_target_multinomial)
    p_multinomial = ft_multinomial.predict(df_train_binomial)

    target_index = p_multinomial.colindex("cat")
    multinomial_model = ft_multinomial.model[:, {
                          "C0" : f[target_index * 2],
                          "C1" : f[target_index * 2 + 1]
                        }]
    assert_equals(
        ft_binomial.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id": dt.int32})
    )
    assert_equals(
        ft_multinomial.labels,
        dt.Frame(label=["cat", "dog"], id=[0, 1], stypes={"id": dt.int32})
    )
    assert ft_binomial.model_type_trained == "binomial"
    assert ft_multinomial.model_type_trained == "multinomial"
    assert_equals(ft_binomial.model, multinomial_model)
    assert_equals(p_binomial[:, 0], p_multinomial[:, 0])


@pytest.mark.parametrize('negative_class', [False, True])
def test_ftrl_fit_predict_multinomial(negative_class):
    negative_class_label = ["_negative_class"] if negative_class else []
    nepochs = 1000
    ft = Ftrl(alpha = 0.2, nepochs = nepochs, double_precision = True)
    ft.negative_class = negative_class
    labels = negative_class_label + ["blue", "green", "red"]

    df_train = dt.Frame(["cucumber", None, "shift", "sky", "day", "orange", "ocean"])
    df_target = dt.Frame(["green", "red", "red", "blue", "green", None, "blue"])
    ft.fit(df_train, df_target)
    frame_integrity_check(ft.model)
    p = ft.predict(df_train)

    frame_integrity_check(p)
    p_none = 1 / p.ncols
    p_dict = p.to_dict()
    p_list = p.to_list()
    sum_p =[sum(row) for row in zip(*p_list)]
    delta_sum = [abs(i - j) for i, j in zip(sum_p, [1] * 5)]
    delta_red =   [abs(i - j) for i, j in
                   zip(p_dict["red"], [0, 1, 1, 0, 0, p_none, 0])]
    delta_green = [abs(i - j) for i, j in
                   zip(p_dict["green"], [1, 0, 0, 0, 1, p_none, 0])]
    delta_blue =  [abs(i - j) for i, j in
                   zip(p_dict["blue"], [0, 0, 0, 1, 0, p_none, 1])]

    ids = [0, 3, 1, 2] if negative_class else [2, 0, 1]
    assert_equals(
        ft.labels,
        dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
    )
    assert ft.model_type_trained == "multinomial"
    assert max(delta_sum)   < 1e-12
    assert max(delta_red)   < epsilon
    assert max(delta_green) < epsilon
    assert max(delta_blue)  < epsilon
    assert list(p.names) == labels


@pytest.mark.parametrize('negative_class', [False, True])
def test_ftrl_fit_predict_multinomial_online(negative_class):
    ft = Ftrl(alpha = 0.2, nepochs = 1000, double_precision = True)
    ft.negative_class = negative_class
    negative_class_label = ["_negative_class"] if negative_class else []

    # Show only 1 label to the model
    df_train = dt.Frame(["cucumber"])
    df_target = dt.Frame(["green"])
    ft.fit(df_train, df_target)

    labels = negative_class_label + ["green"]
    ids = list(range(len(labels)))
    assert_equals(
        ft.labels,
        dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
    )
    assert ft.model_type_trained == "multinomial"
    assert ft.model.shape == (ft.nbins, 2 * ft.labels.nrows)

    # Also do pickling unpickling in the middle.
    ft_pickled = pickle.dumps(ft)
    ft = pickle.loads(ft_pickled)

    # Show one more
    df_train = dt.Frame(["cucumber", None])
    df_target = dt.Frame(["green", "red"])
    ft.fit(df_train, df_target)

    labels += ["red"]
    ids += [len(ids)]
    assert_equals(
        ft.labels,
        dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
    )
    assert ft.model_type_trained == "multinomial"
    assert ft.model.shape == (ft.nbins, 2 * ft.labels.nrows)

    # And one more
    df_train = dt.Frame(["cucumber", None, "shift", "sky", "day", "orange", "ocean"])
    df_target = dt.Frame(["green", "red", "red", "blue", "green", None, "blue"])
    ft.fit(df_train, df_target)

    labels.insert(negative_class, "blue")
    ids.insert(negative_class, len(ids))
    assert_equals(
        ft.labels,
        dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
    )
    assert ft.model_type_trained == "multinomial"
    assert ft.model.shape == (ft.nbins, 2 * ft.labels.nrows)

    # Do not add any new labels
    df_train = dt.Frame(["cucumber", None, "shift", "sky", "day", "orange", "ocean"])
    df_target = dt.Frame(["green", "red", "red", "blue", "green", None, "blue"])

    ft.fit(df_train, df_target)
    assert_equals(
        ft.labels,
        dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
    )
    assert ft.model_type_trained == "multinomial"
    assert ft.model.shape == (ft.nbins, 2 * ft.labels.nrows)

    # Test predictions
    p = ft.predict(df_train)
    frame_integrity_check(p)
    p_none = 1 / p.ncols
    p_dict = p.to_dict()
    p_list = p.to_list()
    sum_p =[sum(row) for row in zip(*p_list)]
    delta_sum = [abs(i - j) for i, j in zip(sum_p, [1] * 5)]
    delta_red =   [abs(i - j) for i, j in
                   zip(p_dict["red"], [0, 1, 1, 0, 0, p_none, 0])]
    delta_green = [abs(i - j) for i, j in
                   zip(p_dict["green"], [1, 0, 0, 0, 1, p_none, 0])]
    delta_blue =  [abs(i - j) for i, j in
                   zip(p_dict["blue"], [0, 0, 0, 1, 0, p_none, 1])]

    assert_equals(
        ft.labels,
        dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
    )
    assert ft.model_type_trained == "multinomial"
    assert max(delta_sum)   < 1e-12
    assert max(delta_red)   < epsilon
    assert max(delta_green) < epsilon
    assert max(delta_blue)  < epsilon
    assert list(p.names) == negative_class_label + ["blue", "green", "red"]


#-------------------------------------------------------------------------------
# Test regression for numerical targets
#-------------------------------------------------------------------------------

# FIXME: we shouldn't start numeric regression when get only None's
def test_ftrl_regression_fit_none():
    ft = Ftrl(nbins = 10, model_type = "regression")
    df_train = dt.Frame(range(ft.nbins))
    df_target = dt.Frame([None] * ft.nbins)
    res = ft.fit(df_train, df_target)
    assert_equals(
        ft.labels,
        dt.Frame(label=["C0"], id=[0], stypes={"id": dt.int32})
    )
    assert ft.model_type == "regression"
    assert ft.model_type_trained == "regression"
    assert res.epoch == 1.0
    assert res.loss is None


def test_ftrl_regression_fit():
    ft = Ftrl(alpha = 2.0, nbins = 10, nepochs = 1000)
    r = list(range(ft.nbins - 1))
    df_train = dt.Frame(r + [0])
    df_target = dt.Frame(r + [math.inf])
    ft.fit(df_train, df_target)
    p = ft.predict(df_train)
    assert_equals(
        ft.labels,
        dt.Frame(label=["C0"], id=[0], stypes={"id": dt.int32})
    )
    delta = [abs(i - j) for i, j in zip(p.to_list()[0], r + [0])]
    assert ft.labels[:, 0].to_list() == [["C0"]]
    assert ft.model_type_trained == "regression"
    assert max(delta) < epsilon


#-------------------------------------------------------------------------------
# Test early stopping
#-------------------------------------------------------------------------------

def test_ftrl_wrong_validation_target_type():
    nepochs = 1234
    nepochs_validation = 56
    nbins = 78
    ft = Ftrl(alpha = 0.5, nbins = nbins, nepochs = nepochs)
    r = range(ft.nbins)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    df_X_val = df_X
    df_y_val = dt.Frame(["Some string data" for _ in r])

    with pytest.raises(TypeError) as e:
        res = ft.fit(df_X, df_y, df_X_val, df_y_val,
                     nepochs_validation = 0)
    assert ("Training and validation target columns must have the same ltype, "
            "got: `integer` and `string`" == str(e.value))


def test_ftrl_wrong_validation_parameters():
    nepochs = 1234
    nepochs_validation = 56
    nbins = 78
    ft = Ftrl(alpha = 0.5, nbins = nbins, nepochs = nepochs)
    r = range(ft.nbins)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)

    with pytest.raises(ValueError) as e:
        res = ft.fit(df_X, df_y, df_X, df_y,
                     nepochs_validation = 0)
    assert ("Argument `nepochs_validation` in Ftrl.fit() should be "
            "positive: 0" == str(e.value))

    with pytest.raises(ValueError) as e:
        res = ft.fit(df_X, df_y, df_X, df_y,
                     validation_average_niterations = 0)
    assert ("Argument `validation_average_niterations` in Ftrl.fit() should be "
            "positive: 0" == str(e.value))


@pytest.mark.parametrize('double_precision_value', [False, True])
def test_ftrl_no_validation_set(double_precision_value):
    nepochs = 1234
    nbins = 56
    ft = Ftrl(alpha = 0.5, nbins = nbins, nepochs = nepochs,
              double_precision = double_precision_value)
    r = range(ft.nbins)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    res = ft.fit(df_X, df_y)
    assert res.epoch == nepochs
    assert res.loss is None


def test_ftrl_no_early_stopping():
    nepochs = 1234
    nepochs_validation = 56
    nbins = 78
    ft = Ftrl(alpha = 0.5, nbins = nbins, nepochs = nepochs)
    r = range(ft.nbins)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    res = ft.fit(df_X, df_y, df_X, df_y,
                 nepochs_validation = nepochs_validation)
    assert res.epoch == nepochs
    assert math.isnan(res.loss) == False


def test_ftrl_early_stopping_different_stypes():
    nepochs = 10000
    nepochs_validation = 5
    nbins = 10
    ft = Ftrl(alpha = 0.5, nbins = nbins, nepochs = nepochs)
    r = range(ft.nbins)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r) # int32 stype
    df_y_val = dt.Frame(list(r)) # int8 stype

    res = ft.fit(df_X, df_y, df_X, df_y_val,
                 nepochs_validation = nepochs_validation
                 )
    p = ft.predict(df_X)
    delta = [abs(i - j) for i, j in zip(p.to_list()[0], list(r))]
    assert res.epoch < nepochs
    assert res.loss < epsilon
    assert int(res.epoch) % nepochs_validation == 0
    assert max(delta) < epsilon


@pytest.mark.parametrize('validation_average_niterations', [1,5,10])
def test_ftrl_early_stopping_int(validation_average_niterations):
    nepochs = 10000
    nepochs_validation = 5
    nbins = 10
    ft = Ftrl(alpha = 0.5, nbins = nbins, nepochs = nepochs)
    r = range(ft.nbins)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    res = ft.fit(df_X, df_y, df_X, df_y,
                 nepochs_validation = nepochs_validation,
                 validation_average_niterations = validation_average_niterations)
    p = ft.predict(df_X)
    delta = [abs(i - j) for i, j in zip(p.to_list()[0], list(r))]
    assert res.epoch < nepochs
    assert res.loss < epsilon
    assert int(res.epoch) % nepochs_validation == 0
    assert max(delta) < epsilon


@pytest.mark.parametrize('validation_average_niterations', [1,5,10])
def test_ftrl_early_stopping_float(validation_average_niterations):
    nepochs = 10000
    nepochs_validation = 5.5
    nbins = 10
    ft = Ftrl(alpha = 0.5, nbins = nbins, nepochs = nepochs)
    r = range(ft.nbins)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    res = ft.fit(df_X, df_y, df_X, df_y,
                 nepochs_validation = nepochs_validation,
                 validation_average_niterations = validation_average_niterations)
    p = ft.predict(df_X)
    delta = [abs(i - j) for i, j in zip(p.to_list()[0], list(r))]
    assert res.epoch < nepochs
    assert res.loss < epsilon
    assert (res.epoch / nepochs_validation ==
            int(res.epoch / nepochs_validation))
    assert max(delta) < epsilon


@pytest.mark.parametrize('validation_average_niterations', [1,5,10])
def test_ftrl_early_stopping_regression(validation_average_niterations):
    nepochs = 10000
    nepochs_validation = 5
    nbins = 10
    ft = Ftrl(alpha = 0.5, nbins = nbins, nepochs = nepochs)
    r = range(ft.nbins)
    df_X_train = dt.Frame(r)
    df_y_train = dt.Frame(r)
    df_X_validate = dt.Frame(range(-nbins, nbins))
    df_y_validate = df_X_validate
    res = ft.fit(df_X_train, df_y_train,
                 df_X_validate[nbins::,:], df_y_validate[nbins::,:],
                 nepochs_validation = nepochs_validation,
                 validation_average_niterations = validation_average_niterations)
    p = ft.predict(df_X_train)
    delta = [abs(i - j) for i, j in zip(p.to_list()[0], list(r))]
    assert res.epoch < nepochs
    assert res.loss < epsilon
    assert int(res.epoch) % nepochs_validation == 0
    assert max(delta) < epsilon


def test_ftrl_early_stopping_multinomial():
    nepochs = 2000
    ft = Ftrl(alpha = 0.2, nepochs = nepochs, double_precision = True)
    labels = ["blue", "green", "red"]

    df_train = dt.Frame(["cucumber", None, "shift", "sky", "day", "orange",
                         "ocean"])
    df_target = dt.Frame(["green", "red", "red", "blue", "green", None,
                          "blue"])
    res = ft.fit(df_train, df_target, df_train[:4, :], df_target[:4, :],
                 nepochs_validation = 1, validation_error = 1e-3)
    frame_integrity_check(ft.model)
    p = ft.predict(df_train)
    frame_integrity_check(p)
    p_none = 1/p.ncols
    p_dict = p.to_dict()
    p_list = p.to_list()
    sum_p =[sum(row) for row in zip(*p_list)]
    delta_sum = [abs(i - j) for i, j in zip(sum_p, [1] * 5)]
    delta_red =   [abs(i - j) for i, j in
                   zip(p_dict["red"], [0, 1, 1, 0, 0, p_none, 0])]
    delta_green = [abs(i - j) for i, j in
                   zip(p_dict["green"], [1, 0, 0, 0, 1, p_none, 0])]
    delta_blue =  [abs(i - j) for i, j in
                   zip(p_dict["blue"], [0, 0, 0, 1, 0, p_none, 1])]

    assert res.epoch < nepochs
    assert res.loss < 0.1
    assert max(delta_sum)   < 1e-6
    assert max(delta_red)   < epsilon
    assert max(delta_green) < epsilon
    assert max(delta_blue)  < epsilon
    assert list(p.names) == labels


#-------------------------------------------------------------------------------
# Test feature importance
#-------------------------------------------------------------------------------

def test_ftrl_feature_importances():
    nrows = 10**4
    feature_names = ['unique', 'boolean', 'mod100']
    ft = Ftrl()
    df_train = dt.Frame([range(nrows),
                         [i % 2 for i in range(nrows)],
                         [i % 100 for i in range(nrows)]
                        ], names = feature_names)
    df_target = dt.Frame([False, True] * (nrows // 2))
    ft.fit(df_train, df_target)
    fi = ft.feature_importances
    assert fi[1].min1() >= 0
    assert math.isclose(fi[1].max1(), 1, abs_tol = 1e-7)
    assert fi.stypes == (stype.str32, stype.float32)
    assert fi.names == ("feature_name", "feature_importance")
    assert fi[0].to_list() == [feature_names]
    assert fi[0, 1] < fi[2, 1]
    assert fi[2, 1] < fi[1, 1]


def test_ftrl_feature_importances_none():
    ft = Ftrl()
    assert ft.feature_importances == None


def test_ftrl_feature_importances_empty():
    ft = Ftrl(nepochs = 0, double_precision = True)
    ft.fit(dt.Frame(range(10)), dt.Frame(range(10)))
    DT = dt.Frame(
      [["C0"], [0.0]],
      names = ("feature_name", "feature_importance")
    )
    assert_equals(ft.feature_importances, DT)


def test_ftrl_fi_shallowcopy():
    import copy
    ft = Ftrl(tparams)
    df_train = dt.Frame(random.sample(range(ft.nbins), ft.nbins))
    df_target = dt.Frame([bool(random.getrandbits(1)) for _ in range(ft.nbins)])
    ft.fit(df_train, df_target)
    fi1 = ft.feature_importances
    fi1 = ft.feature_importances
    assert fi1[1].min1() >= 0
    assert math.isclose(fi1[1].max1(), 1, abs_tol = 1e-7)

    fi2 = copy.deepcopy(ft.feature_importances)
    ft.reset()
    assert ft.feature_importances == None
    assert_equals(fi1, fi2)


#-------------------------------------------------------------------------------
# Test feature interactions
#-------------------------------------------------------------------------------

def test_ftrl_interactions_wrong_features():
    nrows = 10**4
    feature_names = ['unique', 'boolean', 'mod100']
    feature_interactions = [["unique", "boolean"],["unique", "mod1000"]]
    ft = Ftrl()
    ft.interactions = feature_interactions
    df_train = dt.Frame([range(nrows),
                         [i % 2 for i in range(nrows)],
                         [i % 100 for i in range(nrows)]
                        ], names = feature_names)
    df_target = dt.Frame([False, True] * (nrows // 2))
    with pytest.raises(ValueError) as e:
        ft.fit(df_train, df_target)
    assert ("Feature `mod1000` is used in the interactions, however, column "
            "`mod1000` is missing in the training frame" == str(e.value))


@pytest.mark.parametrize("interactions", [
                         [["feature1", "feature2", "feature3"], ["feature3", "feature2"]],
                         [("feature1", "feature2", "feature3"), ("feature3", "feature2")],
                         (["feature1", "feature2", "feature3"], ["feature3", "feature2"]),
                         (("feature1", "feature2", "feature3"), ("feature3", "feature2")),
                        ])
def test_ftrl_interactions_formats(interactions):
    ft = Ftrl(interactions = interactions)
    df_train = dt.Frame(
                 [range(10), [False, True] * 5, range(1, 100, 10)],
                 names = ["feature1", "feature2", "feature3"]
               )
    df_target = dt.Frame(target = [True, False] * 5)

    ft.fit(df_train, df_target)
    fi = ft.feature_importances
    assert fi[1].min1() >= 0
    assert math.isclose(fi[1].max1(), 1, abs_tol = 1e-7)
    assert (fi[0].to_list() ==
           [["feature1", "feature2", "feature3",
             "feature1:feature2:feature3", "feature3:feature2"]])
    assert ft.interactions == tuple(tuple(interaction) for interaction in interactions)


@pytest.mark.parametrize("struct", [list, tuple])
def test_ftrl_interactions_from_itertools(struct):
    import itertools
    df_train = dt.Frame(
                 [range(10), [False, True] * 5, range(1, 100, 10)],
                 names = ["feature1", "feature2", "feature3"]
               )
    df_target = dt.Frame(target = [True, False] * 5)
    interactions = struct(itertools.combinations(df_train.names, 2))

    ft = Ftrl(interactions = interactions)
    ft.fit(df_train, df_target)
    fi = ft.feature_importances
    assert fi[1].min1() >= 0
    assert math.isclose(fi[1].max1(), 1, abs_tol = 1e-7)
    assert (fi[0].to_list() ==
           [["feature1", "feature2", "feature3",
             "feature1:feature2", "feature1:feature3", "feature2:feature3"]])


def test_ftrl_interactions():
    nrows = 10**4
    feature_names = ['unique', 'boolean', 'mod100']
    feature_interactions = (("unique", "boolean"),
                            ("unique", "mod100"),
                            ("boolean", "mod100"),
                            ("boolean", "boolean", "boolean"))
    interaction_names = ["unique:boolean", "unique:mod100",
                         "boolean:mod100", "boolean:boolean:boolean"]
    ft = Ftrl(interactions = feature_interactions)
    df_train = dt.Frame([range(nrows),
                         [i % 2 for i in range(nrows)],
                         [i % 100 for i in range(nrows)]
                        ], names = feature_names)
    df_target = dt.Frame([False, True] * (nrows // 2))
    ft.fit(df_train, df_target)
    fi = ft.feature_importances

    assert fi[1].min1() >= 0
    assert math.isclose(fi[1].max1(), 1, abs_tol = 1e-7)
    assert fi.stypes == (stype.str32, stype.float32)
    assert fi.names == ("feature_name", "feature_importance")
    assert fi[0].to_list() == [feature_names + interaction_names]
    assert fi[0, 1] < fi[2, 1]
    assert fi[2, 1] < fi[1, 1]
    assert fi[3, 1] < fi[1, 1]
    assert fi[4, 1] < fi[1, 1]
    assert fi[5, 1] < fi[1, 1]
    # Make sure interaction of important features is still an important feature
    assert fi[0, 1] < fi[6, 1]
    assert fi[2, 1] < fi[6, 1]
    assert fi[3, 1] < fi[6, 1]
    assert fi[4, 1] < fi[6, 1]
    assert fi[5, 1] < fi[6, 1]

    # Also check what happens when we reset the model
    ft.reset()
    assert ft.interactions == feature_interactions
    ft.interactions = None
    ft.fit(df_train, df_target)
    fi = ft.feature_importances
    assert fi[1].min1() >= 0
    assert math.isclose(fi[1].max1(), 1, abs_tol = 1e-7)
    assert fi[0].to_list() == [feature_names]


#-------------------------------------------------------------------------------
# Test pickling
#-------------------------------------------------------------------------------

def test_ftrl_pickling_empty_model():
    ft_pickled = pickle.dumps(Ftrl())
    ft_unpickled = pickle.loads(ft_pickled)
    assert ft_unpickled.model == None
    assert ft_unpickled.feature_importances == None
    assert ft_unpickled.params == Ftrl().params


def test_ftrl_reuse_pickled_empty_model():
    ft_pickled = pickle.dumps(Ftrl())
    ft_unpickled = pickle.loads(ft_pickled)
    ft_unpickled.nbins = 10
    df_train = dt.Frame({"id" : range(ft_unpickled.nbins)})
    df_target = dt.Frame([True] * ft_unpickled.nbins)
    ft_unpickled.fit(df_train, df_target)
    model = [[0.5] * ft_unpickled.nbins, [0.25] * ft_unpickled.nbins]
    fi = dt.Frame([["id"], [0.0]])[:, [f[0], dt.float32(f[1])]]
    fi.names = ["feature_name", "feature_importance"]
    assert ft_unpickled.model.to_list() == model
    assert_equals(ft_unpickled.feature_importances, fi)


def test_ftrl_pickling_binomial():
    ft = Ftrl(nbins = 10)
    df_train = dt.Frame(range(ft.nbins), names = ["f1"])
    df_target = dt.Frame([True] * ft.nbins)
    ft.fit(df_train, df_target)
    ft_pickled = pickle.dumps(ft)
    ft_unpickled = pickle.loads(ft_pickled)
    frame_integrity_check(ft_unpickled.model)
    assert ft_unpickled.model.stypes == (stype.float32, stype.float32)
    assert_equals(ft.model, ft_unpickled.model)
    assert (ft_unpickled.feature_importances.names ==
            ('feature_name', 'feature_importance',))
    assert (ft_unpickled.feature_importances.stypes ==
            (stype.str32, stype.float32))
    assert_equals(ft.feature_importances, ft_unpickled.feature_importances)
    assert ft.params == ft_unpickled.params
    assert_equals(ft.labels, ft_unpickled.labels)
    assert ft.colnames == ft_unpickled.colnames

    # Predict
    target = ft.predict(df_train)
    target_unpickled = ft_unpickled.predict(df_train)
    assert_equals(ft.model, ft_unpickled.model)
    assert_equals(target, target_unpickled)

    # Fit and predict
    ft_unpickled.fit(df_train, df_target)
    target_unpickled = ft_unpickled.predict(df_train)
    ft.fit(df_train, df_target)
    target = ft.predict(df_train)
    assert_equals(ft.model, ft_unpickled.model)
    assert_equals(target, target_unpickled)


def test_ftrl_pickling_multinomial():
    ft = Ftrl(alpha = 0.2, nbins = 100, nepochs = 1, double_precision = False)
    df_train = dt.Frame(["cucumber", None, "shift", "sky", "day", "orange",
                         "ocean"])
    df_target = dt.Frame(["green", "red", "red", "blue", "green", None,
                          "blue"])
    ft.interactions = [["C0", "C0"]]
    ft.fit(df_train, df_target)

    ft_pickled = pickle.dumps(ft)
    ft_unpickled = pickle.loads(ft_pickled)
    frame_integrity_check(ft_unpickled.model)
    assert ft_unpickled.model.stypes == (stype.float32,) * 6
    assert_equals(ft.model, ft_unpickled.model)
    assert (ft_unpickled.feature_importances.names ==
            ('feature_name', 'feature_importance',))
    assert (ft_unpickled.feature_importances.stypes ==
            (stype.str32, stype.float32))
    assert_equals(ft.feature_importances, ft_unpickled.feature_importances)
    assert ft.params == ft_unpickled.params
    assert_equals(ft.labels, ft_unpickled.labels)
    assert ft.colnames == ft_unpickled.colnames
    assert ft.interactions == ft_unpickled.interactions

    # Predict
    target = ft.predict(df_train)
    target_unpickled = ft_unpickled.predict(df_train)
    assert_equals(ft.model, ft_unpickled.model)
    assert_equals(target, target_unpickled)

    # Fit and predict
    ft.fit(df_train, df_target)
    target = ft.predict(df_train)
    ft_unpickled.fit(df_train, df_target)
    target_unpickled = ft_unpickled.predict(df_train)
    assert_equals(ft.model, ft_unpickled.model)
    assert_equals(target, target_unpickled)


