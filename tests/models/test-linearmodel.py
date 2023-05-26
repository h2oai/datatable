#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2021-2022 H2O.ai
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
# Test `LinearModel` class
#
#-------------------------------------------------------------------------------
import pickle
import pytest
import collections
import random
import math
import datatable as dt
from datatable import f, stype
from datatable.models import LinearModel
from datatable.internal import frame_integrity_check
from datatable.exceptions import DatatableWarning
from tests import assert_equals, noop


#-------------------------------------------------------------------------------
# Define namedtuple of test/default parameters and accuracy
#-------------------------------------------------------------------------------
Params = collections.namedtuple("LinearModelParams",
          ["eta0", "eta_decay", "eta_drop_rate", "eta_schedule",
           "lambda1", "lambda2", "nepochs", "double_precision",
           "negative_class", "model_type", "seed"]
         )

params_test = Params(eta0 = 1,
                 eta_decay = 1.0,
                 eta_drop_rate = 2.0,
                 eta_schedule = 'exponential',
                 lambda1 = 2,
                 lambda2 = 3,
                 nepochs = 4.0,
                 double_precision = True,
                 negative_class = True,
                 model_type = 'binomial',
                 seed = 1)


params_default = Params(eta0 = 0.005,
                        eta_decay = 0.0001,
                        eta_drop_rate = 10.0,
                        eta_schedule = 'constant',
                        lambda1 = 0,
                        lambda2 = 0,
                        nepochs = 1,
                        double_precision = False,
                        negative_class = False,
                        model_type = 'auto',
                        seed = 0)

epsilon = 0.01 # Accuracy required for some tests


#-------------------------------------------------------------------------------
# Test wrong parameter types, names and their combinations in constructor
#-------------------------------------------------------------------------------

def test_linearmodel_construct_wrong_eta_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(eta0 = "1.0"))
    assert ("Argument eta0 in LinearModel() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))


def test_linearmodel_construct_wrong_eta_decay_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(eta_decay = "1.0"))
    assert ("Argument eta_decay in LinearModel() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))


def test_linearmodel_construct_wrong_eta_drop_rate_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(eta_drop_rate = "1.0"))
    assert ("Argument eta_drop_rate in LinearModel() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))


def test_linearmodel_construct_wrong_eta_schedule_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(eta_schedule = 3.14))
    assert ("Argument eta_schedule in LinearModel() constructor should be a string, instead "
            "got <class 'float'>" == str(e.value))


def test_linearmodel_construct_wrong_lambda1_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(lambda1 = "1.0"))
    assert ("Argument lambda1 in LinearModel() constructor should be a float, "
            "instead got <class 'str'>" == str(e.value))


def test_linearmodel_construct_wrong_lambda2_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(lambda2 = "1.0"))
    assert ("Argument lambda2 in LinearModel() constructor should be a float, "
            "instead got <class 'str'>" == str(e.value))


def test_linearmodel_construct_wrong_nepochs_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(nepochs = "10.0"))
    assert ("Argument nepochs in LinearModel() constructor should be a float, "
            "instead got <class 'str'>" == str(e.value))


def test_linearmodel_construct_wrong_double_precision_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(double_precision = 2))
    assert ("Argument double_precision in LinearModel() constructor should be a boolean, "
            "instead got <class 'int'>" == str(e.value))


def test_linearmodel_construct_wrong_seed_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(seed = "123"))
    assert ("Argument seed in LinearModel() constructor should be an integer, "
            "instead got <class 'str'>" == str(e.value))


def test_linearmodel_construct_wrong_combination():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(params=params_test, eta0 = params_test.eta0))
    assert ("You can either pass all the parameters with params or any of "
            "the individual parameters with eta0, eta_decay, eta_drop_rate, "
            "eta_schedule, lambda1, lambda2, nepochs, double_precision, "
            "negative_class, model_type or seed to LinearModel constructor, "
            "but not both at the same time"
            == str(e.value))


def test_linearmodel_construct_unknown_arg():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(c = 1.0))
    assert ("LinearModel() constructor got an unexpected keyword argument c" ==
            str(e.value))


def test_linearmodel_construct_wrong_params_type():
    params = params_test._replace(eta0 = "1.0")
    with pytest.raises(TypeError) as e:
        LinearModel(params)
    assert ("LinearModelParams.eta0 should be a float, instead got <class 'str'>"
            == str(e.value))


def test_linearmodel_construct_wrong_params_name():
    WrongParams = collections.namedtuple("WrongParams",["eta0", "lambda1"])
    wrong_params = WrongParams(eta0 = 1, lambda1 = 0.01)
    with pytest.raises(ValueError) as e:
        LinearModel(wrong_params)
    assert ("Tuple of LinearModel parameters should have 11 elements, instead got: 2"
            == str(e.value))


#-------------------------------------------------------------------------------
# Test wrong parameter values in constructor
#-------------------------------------------------------------------------------

def test_linearmodel_construct_wrong_eta_value():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(eta0 = 0.0))
    assert ("Argument eta0 in LinearModel() constructor should be positive: 0.0"
            == str(e.value))

def test_linearmodel_construct_wrong_eta_decay_value():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(eta_decay = -0.1))
    assert ("Argument eta_decay in LinearModel() constructor should be greater than or equal to zero: -0.1"
            == str(e.value))

def test_linearmodel_construct_wrong_eta_drop_rate_value():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(eta_drop_rate = 0.0))
    assert ("Argument eta_drop_rate in LinearModel() constructor should be positive: 0.0"
            == str(e.value))

def test_linearmodel_construct_wrong_eta_schedule_value():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(eta_schedule = "linear"))
    assert ("Learning rate schedule linear is not supported"
            == str(e.value))

def test_linearmodel_construct_wrong_lambda1_value():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(lambda1 = -1.0))
    assert ("Argument lambda1 in LinearModel() constructor should be greater than "
            "or equal to zero: -1.0" == str(e.value))


def test_linearmodel_construct_wrong_lambda2_value():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(lambda2 = -1.0))
    assert ("Argument lambda2 in LinearModel() constructor should be greater than "
            "or equal to zero: -1.0" == str(e.value))


def test_linearmodel_construct_wrong_nepochs_value():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(nepochs = -1))
    assert ("Argument nepochs in LinearModel() constructor should be greater than or equal to zero: -1"
            == str(e.value))


def test_linearmodel_construct_wrong_seed_value():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(seed = -1))
    assert ("Argument seed in LinearModel() constructor cannot be negative: -1"
            == str(e.value))


#-------------------------------------------------------------------------------
# Test creation of LinearModel object
#-------------------------------------------------------------------------------

def test_linearmodel_create_default():
    lm = LinearModel()
    assert lm.params == params_default
    assert lm.model is None
    assert lm.labels is None


def test_linearmodel_access_params():
    import sys
    lm = LinearModel()
    tuple_refcount = sys.getrefcount(tuple)
    namedtuple_refcount_old = sys.getrefcount(type(lm.params))
    for _ in range(tuple_refcount + 1):
        assert lm.params == params_default
    namedtuple_refcount_new = sys.getrefcount(type(lm.params))
    assert namedtuple_refcount_new == namedtuple_refcount_old


def test_linearmodel_create_params():
    lm = LinearModel(params_test)
    assert lm.params == params_test


def test_linearmodel_create_individual():
    lm = LinearModel(eta0 = params_test.eta0,
              eta_decay = params_test.eta_decay,
              eta_drop_rate = params_test.eta_drop_rate,
              eta_schedule = params_test.eta_schedule,
              lambda1 = params_test.lambda1, lambda2 = params_test.lambda2,
              nepochs = params_test.nepochs,
              double_precision = params_test.double_precision,
              negative_class = params_test.negative_class,
              model_type = params_test.model_type,
              seed = params_test.seed)
    assert lm.params == (params_test.eta0,
                         params_test.eta_decay,
                         params_test.eta_drop_rate,
                         params_test.eta_schedule,
                         params_test.lambda1, params_test.lambda2,
                         params_test.nepochs,
                         params_test.double_precision, params_test.negative_class,
                         params_test.model_type,
                         params_test.seed)


#-------------------------------------------------------------------------------
# Test getters and setters for valid LineamModel parameters
#-------------------------------------------------------------------------------

def test_linearmodel_get_params():
    lm = LinearModel(params_test)
    params = lm.params
    assert params == params_test
    assert (lm.eta0, lm.eta_decay, lm.eta_drop_rate, lm.eta_schedule,
           lm.lambda1, lm.lambda2,
           lm.nepochs, lm.double_precision, lm.negative_class,
           lm.model_type, lm.seed) == params_test
    assert (params.eta0, params.eta_decay, params.eta_drop_rate, params.eta_schedule,
            params.lambda1, params.lambda2,
            params.nepochs, params.double_precision,
            params.negative_class, params.model_type,
            params.seed) == params_test


def test_linearmodel_set_individual():
    lm = LinearModel(double_precision = params_test.double_precision)
    lm.eta0 = params_test.eta0
    lm.eta_decay = params_test.eta_decay
    lm.eta_drop_rate = params_test.eta_drop_rate
    lm.eta_schedule = params_test.eta_schedule
    lm.lambda1 = params_test.lambda1
    lm.lambda2 = params_test.lambda2
    lm.nepochs = params_test.nepochs
    lm.negative_class = params_test.negative_class
    lm.model_type = params_test.model_type
    lm.seed = params_test.seed
    assert lm.params == params_test


def test_linearmodel_set_individual_after_params():
    lm = LinearModel()
    params = lm.params
    lm.eta0 = params_test.eta0
    params_new = lm.params
    assert params == LinearModel().params
    assert (params_new.eta0, params_new.eta_decay, params_new.eta_drop_rate,
            params_new.eta_schedule,
            params_new.lambda1, params_new.lambda2,
            params_new.nepochs,
            params_new.double_precision, params_new.negative_class,
            params_new.model_type, params_new.seed) == params_new
    assert (lm.eta0, lm.eta_decay, lm.eta_drop_rate, lm.eta_schedule,
            lm.lambda1, lm.lambda2,
            lm.nepochs,
            lm.double_precision, lm.negative_class,
            lm.model_type, lm.seed) == params_new


#-------------------------------------------------------------------------------
# Test getters and setters for wrong types / names of LinearModel parameters
#-------------------------------------------------------------------------------

def test_linearmodel_set_wrong_eta_type():
    lm = LinearModel()
    with pytest.raises(TypeError) as e:
        lm.eta0 = "0.0"
    assert (".eta0 should be a float, instead got <class 'str'>" == str(e.value))


def test_linearmodel_set_wrong_lambda1_type():
    lm = LinearModel()
    with pytest.raises(TypeError) as e:
        lm.lambda1 = "-1.0"
    assert (".lambda1 should be a float, instead got <class 'str'>" == str(e.value))


def test_linearmodel_set_wrong_lambda2_type():
    lm = LinearModel()
    with pytest.raises(TypeError) as e:
        lm.lambda2 = "-1.0"
    assert (".lambda2 should be a float, instead got <class 'str'>" == str(e.value))


def test_linearmodel_set_wrong_nepochs_type():
    lm = LinearModel()
    with pytest.raises(TypeError) as e:
        lm.nepochs = "-10.0"
    assert (".nepochs should be a float, instead got <class 'str'>" == str(e.value))


def test_linearmodel_set_wrong_seed_type():
    lm = LinearModel()
    with pytest.raises(TypeError) as e:
        lm.seed = "-10.0"
    assert (".seed should be an integer, instead got <class 'str'>" == str(e.value))


#-------------------------------------------------------------------------------
# Test getters and setters for wrong values of individual LinearModel parameters
#-------------------------------------------------------------------------------

@pytest.mark.parametrize('value, message',
                         [[0.0, ".eta0 should be positive: 0.0"],
                          [None, ".eta0 should be positive: None"],
                          [math.nan, ".eta0 should be positive: nan"],
                          [math.inf, ".eta0 should be finite: inf"]
                         ])
def test_linearmodel_set_bad_eta_value(value, message):
    lm = LinearModel()
    with pytest.raises(ValueError) as e:
        lm.eta0 = value
    assert (message == str(e.value))


@pytest.mark.parametrize('value, message',
                         [[-1.0, ".lambda1 should be greater than or equal to zero: -1.0"],
                          [None, ".lambda1 should be greater than or equal to zero: None"],
                          [math.nan, ".lambda1 should be greater than or equal to zero: nan"],
                          [math.inf, ".lambda1 should be finite: inf"]
                         ])
def test_linearmodel_set_bad_lambda1_value(value, message):
    lm = LinearModel()
    with pytest.raises(ValueError) as e:
        lm.lambda1 = value
    assert (message == str(e.value))


@pytest.mark.parametrize('value, message',
                         [[-1.0, ".lambda2 should be greater than or equal to zero: -1.0"],
                          [None, ".lambda2 should be greater than or equal to zero: None"],
                          [math.nan, ".lambda2 should be greater than or equal to zero: nan"],
                          [math.inf, ".lambda2 should be finite: inf"]
                         ])
def test_linearmodel_set_bad_lambda2_value(value, message):
    lm = LinearModel()
    with pytest.raises(ValueError) as e:
        lm.lambda2 = value
    assert (message == str(e.value))


def test_linearmodel_set_wrong_nepochs_value():
    lm = LinearModel()
    with pytest.raises(ValueError) as e:
        lm.nepochs = -10
    assert (".nepochs should be greater than or equal to zero: -10" == str(e.value))


#-------------------------------------------------------------------------------
# Test wrong training frame
#-------------------------------------------------------------------------------

def test_linearmodel_fit_wrong_empty_training():
    lm = LinearModel()
    df_train = dt.Frame()
    df_target = dt.Frame([True])
    with pytest.raises(ValueError) as e:
        lm.fit(df_train, df_target)
    assert ("Training frame must have at least one column" ==
            str(e.value))


def test_linearmodel_fit_wrong_empty_target():
    lm = LinearModel()
    df_train = dt.Frame([1.0, 2.0])
    df_target = dt.Frame()
    with pytest.raises(ValueError) as e:
        lm.fit(df_train, df_target)
    assert ("Target frame must have exactly one column" ==
            str(e.value))


def test_linearmodel_fit_wrong_target_obj64():
    lm = LinearModel()
    df_train = dt.Frame(list(range(8)))
    df_target = dt.Frame([3, "point", None, None, 14, 15, {92}, "6"],
                         stype=dt.obj64)
    with pytest.raises(TypeError) as e:
        lm.fit(df_train, df_target)
    assert ("Target column should have one of the following ltypes: "
            "void, bool, int, real or string, instead got: object" ==
            str(e.value))


#-------------------------------------------------------------------------------
# Test wrong parameters for `fit()` and `predict()` methods
#-------------------------------------------------------------------------------

def test_linearmodel_model_untrained():
    lm = LinearModel()
    assert lm.model is None


def test_linearmodel_fit_no_frame():
    lm = LinearModel()
    with pytest.raises(ValueError) as e:
        lm.fit()
    assert ("Training frame parameter is missing"
            == str(e.value))


def test_linearmodel_fit_no_target():
    lm = LinearModel()
    with pytest.raises(ValueError) as e:
        lm.fit(None)
    assert ("Target frame parameter is missing"
            == str(e.value))


def test_linearmodel_fit_predict_nones():
    lm = LinearModel()
    lm.fit(None, None)
    df_target = lm.predict(None)
    assert df_target is None


def test_linearmodel_predict_not_trained():
    lm = LinearModel()
    df_train = dt.Frame([[1, 2, 3], [True, False, True]])
    with pytest.raises(ValueError) as e:
        lm.predict(df_train)
    assert ("Cannot make any predictions, the model should be trained first"
            == str(e.value))


def test_linearmodel_predict_wrong_frame():
    lm = LinearModel()
    df_train = dt.Frame([[1, 2, 3]])
    df_target = dt.Frame([[True, False, True]])
    df_predict = dt.Frame([[1, 2, 3], [4, 5, 6]])
    lm.fit(df_train, df_target)
    with pytest.raises(ValueError) as e:
        lm.predict(df_predict)
    assert ("Can only predict on a frame that has 1 column, "
            "i.e. the same number of features the model was trained on"
            == str(e.value))


#-------------------------------------------------------------------------------
# Test `reset()` method
#-------------------------------------------------------------------------------

def test_linearmodel_reset_untrained():
    lm = LinearModel(params_test)
    assert lm.model is None
    assert lm.params == params_test
    lm.reset()
    assert lm.model is None
    assert lm.params == params_test


def test_linearmodel_reset_trained():
    lm = LinearModel(params_test)
    lm.fit(dt.Frame(range(10)), dt.Frame([False, True] * 5))
    assert lm.model is not None
    lm.reset()
    assert lm.model is None
    assert lm.params == params_test


#-------------------------------------------------------------------------------
# Test `fit()` and `predict()` methods for binomial classification
#-------------------------------------------------------------------------------

def test_linearmodel_binomial_fit_none():
    nrows = 10
    lm = LinearModel(model_type = "binomial")
    df_train = dt.Frame(range(nrows))
    df_target = dt.Frame([None] * nrows)
    assert lm.is_fitted() == False
    res = lm.fit(df_train, df_target)
    assert lm.model is None
    assert not lm.labels
    assert lm.model_type == "binomial"
    assert lm.is_fitted() == False
    assert res.epoch == 0.0
    assert res.loss is None


def test_linearmodel_binomial_fit_predict_ignore_none():
    nrows = 10
    lm = LinearModel()
    df_train = dt.Frame([list(range(2 * nrows)), list(range(2 * nrows))])
    df_target = dt.Frame([True] * nrows + [None] * nrows)
    lm.fit(df_train, df_target)
    df_predict = dt.Frame([[1], [None]])
    p = lm.predict(df_predict)
    assert_equals(
        lm.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert_equals(
        p,
        dt.Frame({"False":[None]/stype.float32, "True":[None]/stype.float32})
    )
    assert lm.is_fitted() == True
    assert lm.model_type == "binomial"


def test_linearmodel_binomial_fit_predict_bool():
    lm = LinearModel(eta0 = 0.1, nepochs = 10000)
    df_train = dt.Frame([[True, False]])
    df_target = dt.Frame([[True, False]])
    lm.fit(df_train, df_target)
    df_target = lm.predict(df_train[:,0])
    assert_equals(
        lm.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert lm.model_type == "binomial"
    assert df_target[0, 1] <= 1
    assert df_target[0, 1] >= 1 - epsilon
    assert df_target[1, 1] >= 0
    assert df_target[1, 1] < epsilon


def test_linearmodel_binomial_fit_predict_int():
    lm = LinearModel(eta0 = 0.1, nepochs = 10000)
    df_train = dt.Frame([[0, 1]])
    df_target = dt.Frame([True, False])
    lm.fit(df_train, df_target)
    df_target = lm.predict(df_train[:,0])
    assert lm.model_type == "binomial"
    assert df_target[0, 1] <= 1
    assert df_target[0, 1] >= 1 - epsilon
    assert df_target[1, 1] >= 0
    assert df_target[1, 1] < epsilon


def test_linearmodel_binomial_fit_predict_float():
    lm = LinearModel(eta0 = 0.1, nepochs = 10000)
    df_train = dt.Frame([[0.0, 1.0, math.inf]])
    df_target = dt.Frame([[True, False, False]])
    lm.fit(df_train, df_target)
    df_target = lm.predict(df_train[:,0])
    assert lm.model_type == "binomial"
    assert df_target[0, 1] <= 1
    assert df_target[0, 1] >= 1 - epsilon
    assert df_target[1, 1] >= 0
    assert df_target[1, 1] < epsilon


@pytest.mark.parametrize('target',
                         [[True, False],
                         ["yes", "no"],
                         [20, 10],
                         [0.5, -0.5]])
def test_linearmodel_binomial_fit_predict_various(target):
    lm = LinearModel(eta0 = 0.1, nepochs = 10000, model_type = "binomial")
    df_train = dt.Frame([True, False])
    df_target = dt.Frame(target)
    lm.fit(df_train, df_target)
    df_res = lm.predict(df_train)
    target.sort()
    ids = [1, 0]
    # When a target column has a boolean type, negatives
    # will always be assigned a `0` label id.
    if lm.labels.stypes[0] == dt.stype.bool8:
        ids.sort()
    assert_equals(
        lm.labels,
        dt.Frame(label=target, id=ids, stypes={"id" : stype.int32})
    )
    assert lm.labels[:, 0].to_list() == [sorted(target)]
    assert lm.model_type == "binomial"
    assert df_res[0, 1] <= 1
    assert df_res[0, 1] >= 1 - epsilon
    assert df_res[1, 1] >= 0
    assert df_res[1, 1] < epsilon
    assert df_res[0, 0] >= 0
    assert df_res[0, 0] < epsilon
    assert df_res[1, 0] <= 1
    assert df_res[1, 0] >= 1 - epsilon


def test_linearmodel_binomial_fit_predict_view():
    nrows = 100
    lm = LinearModel()
    df_train = dt.Frame(random.sample(range(nrows), nrows))
    df_target = dt.Frame([bool(random.getrandbits(1)) for _ in range(nrows)])
    rows = range(nrows//2, nrows)

    # Train model and predict on a view
    lm.fit(df_train[rows,:], df_target[rows,:])
    p_view = lm.predict(df_train[rows,:])
    model_view = lm.model

    # Train model and predict on a frame
    lm.reset()
    df_train_range = df_train[rows,:]
    df_target_range = df_target[rows,:]
    df_train_range.materialize()
    df_target_range.materialize()
    lm.fit(df_train_range, df_target_range)
    p_frame = lm.predict(df_train_range)

    assert_equals(
        lm.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert lm.model_type == "binomial"
    assert_equals(model_view, lm.model, rel_tol = 1e-6)
    assert_equals(p_view, p_frame, rel_tol = 1e-6)


@pytest.mark.parametrize('parameter, value',
                         [("negative_class", True)])
def test_linearmodel_binomial_disable_setters_after_fit(parameter, value):
    nrows = 10
    lm = LinearModel()
    df_train = dt.Frame(range(nrows))
    df_target = dt.Frame([True] * nrows)
    lm.fit(df_train, df_target)

    assert lm.model_type == "binomial"
    with pytest.raises(ValueError) as e:
        setattr(lm, parameter, value)
    assert ("Cannot change ."+parameter+" for a trained model, "
            "reset this model or create a new one"
            == str(e.value))
    lm.reset()
    setattr(lm, parameter, value)


# def test_linearmodel_binomial_fit_predict_online_1_1():
#     lm = LinearModel(eta0 = 0.1, nepochs = 10000, model_type = "binomial")
#     df_train_odd = dt.Frame([[1, 3, 7, 5, 9]])
#     df_target_odd = dt.Frame([["odd", "odd", "odd", "odd", "odd"]])
#     lm.fit(df_train_odd, df_target_odd)
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=["odd"], id=[0], stypes={"id": dt.int32}
#     )
#                   )

#     df_train_even = dt.Frame([[2, 4, 8, 6]])
#     df_target_even = dt.Frame([["even", "even", "even", "even"]])
#     lm.fit(df_train_even, df_target_even)
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=["even", "odd"], id=[1, 0], stypes={"id": dt.int32})
#     )

#     df_train_wrong = dt.Frame([[2, 4, None, 6]])
#     df_target_wrong = dt.Frame([["even", "even", "none", "even"]])
#     with pytest.raises(ValueError) as e:
#         lm.fit(df_train_wrong, df_target_wrong)
#     assert ("Got a new label in the target column, however, both positive and "
#             "negative labels are already set"
#             == str(e.value))

#     p = lm.predict(df_train_odd)
#     p_dict = p.to_dict()
#     delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [1, 1, 1, 1, 1])]
#     delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [0, 0, 0, 0, 0])]
#     assert lm.model_type == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon

#     p = lm.predict(df_train_even)
#     p_dict = p.to_dict()
#     delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 1, 1, 1])]
#     delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 0, 0, 0])]
#     assert lm.model_type == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon


# def test_linearmodel_binomial_fit_predict_online_1_2():
#     lm = LinearModel(eta0 = 0.1, nepochs = 10000, model_type = "binomial")
#     df_train_odd = dt.Frame([[1, 3, 7, 5, 9]])
#     df_target_odd = dt.Frame([["odd", "odd", "odd", "odd", "odd"]])
#     lm.fit(df_train_odd, df_target_odd)
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=["odd"], id=[0], stypes={"id": dt.int32})
#     )

#     df_train_wrong = dt.Frame([[2, 4, None, 6]])
#     df_target_wrong = dt.Frame([["even", "even", "none", "even"]])
#     with pytest.raises(ValueError) as e:
#         lm.fit(df_train_wrong, df_target_wrong)
#     assert ("Got two new labels in the target column, however, positive "
#             "label is already set"
#             == str(e.value))

#     df_train_even_odd = dt.Frame([[2, 1, 8, 3]])
#     df_target_even_odd = dt.Frame([["even", "odd", "even", "odd"]])
#     lm.fit(df_train_even_odd, df_target_even_odd)
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=["even", "odd"], id=[1, 0], stypes={"id": dt.int32})
#     )

#     p = lm.predict(df_train_odd)
#     p_dict = p.to_dict()
#     delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [1, 1, 1, 1, 1])]
#     delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [0, 0, 0, 0, 0])]
#     assert lm.model_type == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon

#     p = lm.predict(df_train_even_odd)
#     p_dict = p.to_dict()
#     delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 0, 1, 0])]
#     delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 1, 0, 1])]
#     assert lm.model_type == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon


# def test_linearmodel_binomial_fit_predict_online_2_1():
#     lm = LinearModel(eta0 = 0.1, nepochs = 10000, model_type = "binomial")
#     df_train_even_odd = dt.Frame([[2, 1, 8, 3]])
#     df_target_even_odd = dt.Frame([["even", "odd", "even", "odd"]])
#     lm.fit(df_train_even_odd, df_target_even_odd)
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=["even", "odd"], id=[0, 1], stypes={"id": dt.int32})
#     )

#     df_train_odd = dt.Frame([[1, 3, 7, 5, 9]])
#     df_target_odd = dt.Frame([["odd", "odd", "odd", "odd", "odd"]])
#     lm.fit(df_train_odd, df_target_odd)
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=["even", "odd"], id=[0, 1], stypes={"id": dt.int32})
#     )

#     df_train_wrong = dt.Frame([[math.inf, math.inf, None, math.inf]])
#     df_target_wrong = dt.Frame([["inf", "inf", "none", "inf"]])
#     with pytest.raises(ValueError) as e:
#         lm.fit(df_train_wrong, df_target_wrong)
#     assert ("Got a new label in the target column, however, both positive and "
#             "negative labels are already set"
#             == str(e.value))


#     p = lm.predict(df_train_odd)
#     p_dict = p.to_dict()
#     delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [1, 1, 1, 1, 1])]
#     delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [0, 0, 0, 0, 0])]
#     assert lm.model_type == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon

#     p = lm.predict(df_train_even_odd)
#     p_dict = p.to_dict()
#     delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 0, 1, 0])]
#     delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 1, 0, 1])]
#     assert lm.model_type == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon


# def test_linearmodel_binomial_fit_predict_online_2_2():
#     lm = LinearModel(eta0 = 0.1, nepochs = 10000, model_type = "binomial")
#     df_train_even_odd = dt.Frame([[2, 1, 8, 3]])
#     df_target_even_odd = dt.Frame([["even", "odd", "even", "odd"]])
#     lm.fit(df_train_even_odd, df_target_even_odd)
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=["even", "odd"], id=[0, 1], stypes={"id": dt.int32})
#     )

#     df_train_odd_even = dt.Frame([[1, 2, 3, 8]])
#     df_target_odd_even = dt.Frame([["odd", "even", "odd", "even"]])
#     lm.fit(df_train_odd_even, df_target_odd_even)
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=["even", "odd"], id=[0, 1], stypes={"id": dt.int32})
#     )

#     df_train_wrong = dt.Frame([[math.inf, math.inf, None, math.inf]])
#     df_target_wrong = dt.Frame([["inf", "inf", "none", "inf"]])
#     with pytest.raises(ValueError) as e:
#         lm.fit(df_train_wrong, df_target_wrong)
#     assert ("Got a new label in the target column, however, both positive and "
#             "negative labels are already set"
#             == str(e.value))

#     p = lm.predict(df_train_even_odd)
#     p_dict = p.to_dict()
#     delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 0, 1, 0])]
#     delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 1, 0, 1])]
#     assert lm.model_type == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon


#-------------------------------------------------------------------------------
# Test multinomial regression
#-------------------------------------------------------------------------------

def test_linearmodel_multinomial_fit_ignore_none():
    nrows = 10
    lm = LinearModel(model_type = "multinomial")
    df_train = dt.Frame(range(nrows))
    df_target = dt.Frame([None] * nrows)
    res = lm.fit(df_train, df_target)
    assert not lm.labels
    assert lm.model_type == "multinomial"
    assert lm.is_fitted() == False
    assert res.epoch == 0.0
    assert res.loss is None


def test_linearmodel_multinomial_fit_predict_none():
    lm = LinearModel()
    df_train = dt.Frame([[1, 2, 3], [4, 5, 6], [7, 8, 9]])
    df_target = dt.Frame(["cat", "dog", "mouse"])
    res = lm.fit(df_train, df_target)
    p = lm.predict(dt.Frame([[1], [None], [2]]))
    assert lm.model_type == "multinomial"
    assert_equals(
        lm.labels,
        dt.Frame(label=["cat", "dog", "mouse"], id=[0, 1, 2]/dt.int32)
    )
    assert_equals(
        p,
        dt.Frame(
            cat=[None]/dt.float32,
            dog=[None]/dt.float32,
            mouse=[None]/dt.float32
        )
    )
    assert lm.is_fitted() == True


def test_linearmodel_multinomial_vs_binomial_fit_predict():
    target_names = ["cat", "dog"]
    lm_binomial = LinearModel(nepochs = 1, seed = 123)
    df_train_binomial = dt.Frame(range(10))
    df_target_binomial = dt.Frame({"target" : [False, True] * 5})
    lm_binomial.fit(df_train_binomial, df_target_binomial)
    p_binomial = lm_binomial.predict(df_train_binomial)
    p_binomial.names = target_names

    lm_multinomial = LinearModel(nepochs = 1, model_type = "multinomial", seed = 123)
    df_target_multinomial = dt.Frame(target_names * 5)
    lm_multinomial.fit(df_train_binomial, df_target_multinomial)
    p_multinomial = lm_multinomial.predict(df_train_binomial)

    target_index = p_multinomial.colindex("cat")
    multinomial_model = lm_multinomial.model[:, {
                          "C0" : f[target_index ]
                        }]
    assert_equals(
        lm_binomial.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id": dt.int32})
    )
    assert_equals(
        lm_multinomial.labels,
        dt.Frame(label=["cat", "dog"], id=[0, 1], stypes={"id": dt.int32})
    )
    assert lm_binomial.model_type == "binomial"
    assert lm_multinomial.model_type == "multinomial"
    assert_equals(lm_binomial.model, multinomial_model)
    assert_equals(p_binomial[:, 0], p_multinomial[:, 0])


#-------------------------------------------------------------------------------
# Test regression for numerical targets
#-------------------------------------------------------------------------------

@pytest.mark.parametrize('eta_schedule', ["constant", "time-based", "step-based", "exponential"])
def test_linearmodel_regression_fit_none(eta_schedule):
    nrows = 10
    lm = LinearModel(model_type = "regression", eta_schedule=eta_schedule)
    df_train = dt.Frame(range(nrows))
    df_target = dt.Frame([None] * nrows)
    res = lm.fit(df_train, df_target)
    assert_equals(
        lm.labels,
        dt.Frame(label=["C0"], id=[0], stypes={"id": dt.int32})
    )
    assert lm.model_type == "regression"
    assert res.epoch == 1.0
    assert res.loss is None


@pytest.mark.parametrize('eta_schedule', ["constant", "time-based", "step-based", "exponential"])
def test_linearmodel_regression_fit_simple_zero(eta_schedule):
    lm = LinearModel(nepochs = 1, double_precision = True, eta_schedule=eta_schedule)
    df_train = dt.Frame([0])
    df_target = dt.Frame([0])
    res = lm.fit(df_train, df_target)
    assert lm.model_type == "regression"
    assert_equals(lm.model, dt.Frame([0.0, 0.0]))


@pytest.mark.parametrize('eta_schedule', ["constant", "time-based", "step-based", "exponential"])
def test_linearmodel_regression_fit_simple_one(eta_schedule):
    lm = LinearModel(nepochs = 1, double_precision = True, eta_schedule=eta_schedule)
    df_train = dt.Frame([1])
    df_target = dt.Frame([1])
    lm.fit(df_train, df_target)
    assert lm.model_type == "regression"
    assert_equals(lm.model, dt.Frame([lm.eta0, lm.eta0]), rel_tol = 1e-3)


@pytest.mark.parametrize('eta_schedule', ["constant", "time-based", "exponential"])
def test_linearmodel_regression_fit_simple_one_multicolumn(eta_schedule):
    lm = LinearModel(nepochs = 1000, double_precision = True, eta_schedule=eta_schedule)
    df_train = dt.Frame([[1], [2], [3], [4], [5]])
    df_target = dt.Frame([1]/dt.float64)
    lm.fit(df_train, df_target)
    df_predict = dt.Frame([[1, 2], [2, None], [3, 4], [4, 5], [5, 6]])
    p = lm.predict(df_predict)
    assert lm.model_type == "regression"
    assert_equals(p, dt.Frame([1, None]/dt.float64))


@pytest.mark.parametrize('eta_schedule', ["constant", "time-based", "step-based", "exponential"])
def test_linearmodel_regression_fit_predict_simple_linear(eta_schedule):
    lm = LinearModel(nepochs = 10000, double_precision = True, eta_decay = 1e-8,
                     eta_drop_rate = 5000, eta_schedule=eta_schedule)
    df_train = dt.Frame([1, 2])
    df_target = dt.Frame([1, 2])
    lm.fit(df_train, df_target)
    assert lm.model_type == "regression"
    p = lm.predict(dt.Frame([1.5]))
    assert_equals(p, dt.Frame([1.5]), rel_tol = 1e-3)


@pytest.mark.parametrize('eta_schedule', ["constant", "time-based", "step-based", "exponential"])
def test_linearmodel_regression_fit_predict_simple_one_epoch(eta_schedule):
    lm = LinearModel(nepochs = 1, eta_schedule=eta_schedule)
    r = list(range(10))
    df_train = dt.Frame(r)
    df_target = dt.Frame(r/dt.float32)
    lm.fit(df_train, df_target)
    assert lm.is_fitted() == True
    assert lm.model_type == "regression"
    p = lm.predict(df_train)
    assert_equals(
        lm.labels,
        dt.Frame(label=["C0"], id=[0], stypes={"id": dt.int32})
    )


@pytest.mark.parametrize('eta_schedule', ["constant", "time-based", "step-based", "exponential"])
def test_linearmodel_regression_fit_predict(eta_schedule):
    lm = LinearModel(nepochs = 10000, eta_drop_rate = 1000,
                     eta_schedule=eta_schedule, eta_decay = 1e-7)
    r = list(range(9))
    df_train = dt.Frame(r + [0])
    df_target = dt.Frame(r + [math.inf])
    lm.fit(df_train, df_target)
    p = lm.predict(df_train)
    assert_equals(
        lm.labels,
        dt.Frame(label=["C0"], id=[0], stypes={"id": dt.int32})
    )
    assert lm.labels[:, 0].to_list() == [["C0"]]
    assert lm.model_type == "regression"
    assert_equals(p, df_train[:, dt.float32(f[0])], rel_tol = 1e-6)


def test_linearmodel_regression_fit_predict_large():
    N = 40000
    lm = LinearModel(eta0 = 1e-4, nepochs = 100, double_precision = True)

    df_train = dt.Frame([range(N), range(0, 2*N - 1, 2)])
    df_train_standard = df_train[:, (f[:] - dt.mean(f[:])) / dt.sd(f[:])]
    df_target = df_train[:, dt.float64(1 - f[0] + 2 * f[1])]

    lm.fit(df_train_standard, df_target)
    p = lm.predict(df_train_standard)
    assert_equals(df_target, p, rel_tol = 1e-6)


#-------------------------------------------------------------------------------
# Test early stopping
#-------------------------------------------------------------------------------

def test_linearmodel_early_stopping_wrong_validation_target_type():
    nepochs = 1234
    nepochs_validation = 56
    nrows = 78
    lm = LinearModel(eta0 = 0.5, nepochs = nepochs)
    r = range(nrows)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    df_X_val = df_X
    df_y_val = dt.Frame(["Some string data" for _ in r])

    with pytest.raises(TypeError) as e:
        res = lm.fit(df_X, df_y, df_X_val, df_y_val,
                     nepochs_validation = 0)
    assert ("Training and validation target columns must have the same ltype, "
            "got: int and str" == str(e.value))


def test_linearmodel_early_stopping_wrong_validation_parameters():
    nepochs = 1234
    nepochs_validation = 56
    nrows = 78
    lm = LinearModel(eta0 = 0.5, nepochs = nepochs)
    r = range(nrows)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)

    with pytest.raises(ValueError) as e:
        res = lm.fit(df_X, df_y, df_X, df_y,
                     nepochs_validation = 0)
    assert ("Argument nepochs_validation in LinearModel.fit() should be "
            "positive: 0" == str(e.value))

    with pytest.raises(ValueError) as e:
        res = lm.fit(df_X, df_y, df_X, df_y,
                     validation_average_niterations = 0)
    assert ("Argument validation_average_niterations in LinearModel.fit() should be "
            "positive: 0" == str(e.value))


@pytest.mark.parametrize('double_precision_value', [False, True])
def test_linearmodel_early_stopping_no_validation_set(double_precision_value):
    nepochs = 1234
    nrows = 56
    lm = LinearModel(eta0 = 0.5, nepochs = nepochs,
                     double_precision = double_precision_value)
    r = range(nrows)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    res = lm.fit(df_X, df_y)
    assert res.epoch == nepochs
    assert res.loss is None


def test_linearmodel_early_stopping_not_triggered():
    nepochs = 100
    nepochs_validation = 10
    lm = LinearModel(nepochs = nepochs)
    df_X = dt.Frame([1])
    df_y = dt.Frame([True])
    res = lm.fit(df_X, df_y, df_X, df_y,
                 nepochs_validation = nepochs_validation)
    assert res.epoch == nepochs
    assert math.isnan(res.loss) == False


def test_linearmodel_early_stopping_wrong_validation_stypes():
    lm = LinearModel(eta0 = 0.5)
    r = range(10)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r) # int32 stype
    df_X_val = dt.Frame(["1"] * 10)
    df_y_val = dt.Frame(list(r)) # int8 stype

    msg = r"Training and validation frames must have identical column ltypes, " \
           "instead for columns C0 and C0, got ltypes: int and str"
    with pytest.raises(TypeError, match=msg):
        res = lm.fit(df_X, df_y, df_X_val, df_y_val)


def test_linearmodel_early_stopping_compatible_stypes():
    nepochs = 10000
    nepochs_validation = 5
    lm = LinearModel(nepochs = nepochs)
    r = range(10)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r) # int32 stype
    df_y_val = dt.Frame(list(r)/dt.int8) # int8 stype

    res = lm.fit(df_X, df_y, df_X, df_y_val,
                 nepochs_validation = nepochs_validation
                 )

    p = lm.predict(df_X)
    delta = [abs(i - j) for i, j in zip(p.to_list()[0], list(r))]
    assert res.epoch < nepochs
    assert res.loss < epsilon
    assert int(res.epoch) % nepochs_validation == 0
    assert max(delta) < epsilon


@pytest.mark.parametrize('validation_average_niterations', [1,5,10])
def test_linearmodel_early_stopping_int(validation_average_niterations):
    nepochs = 10000
    nepochs_validation = 5
    lm = LinearModel(nepochs = nepochs)
    r = range(10)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    res = lm.fit(df_X, df_y, df_X, df_y,
                 nepochs_validation = nepochs_validation,
                 validation_average_niterations = validation_average_niterations)
    p = lm.predict(df_X)
    delta = [abs(i - j) for i, j in zip(p.to_list()[0], list(r))]
    assert res.epoch < nepochs
    assert res.loss < epsilon
    assert int(res.epoch) % nepochs_validation == 0
    assert max(delta) < epsilon


@pytest.mark.parametrize('validation_average_niterations', [1,5,10])
def test_linearmodel_early_stopping_float(validation_average_niterations):
    nepochs = 10000
    nepochs_validation = 5.5
    lm = LinearModel(nepochs = nepochs)
    r = range(10)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    res = lm.fit(df_X, df_y, df_X, df_y,
                 nepochs_validation = nepochs_validation,
                 validation_average_niterations = validation_average_niterations)
    p = lm.predict(df_X)
    delta = [abs(i - j) for i, j in zip(p.to_list()[0], list(r))]
    assert res.epoch < nepochs
    assert res.loss < epsilon
    assert (res.epoch / nepochs_validation ==
            int(res.epoch / nepochs_validation))
    assert max(delta) < epsilon

    # Re-create the same model without early stopping and also test
    # training for a fractional number of epochs.
    lm1 = LinearModel(nepochs = res.epoch)
    lm1.fit(df_X, df_y)
    p1 = lm1.predict(df_X)
    assert_equals(lm.model, lm1.model)
    assert_equals(p, p1)


@pytest.mark.parametrize('validation_average_niterations', [1,5,10])
def test_linearmodel_early_stopping_regression(validation_average_niterations):
    nepochs = 10000
    nepochs_validation = 5
    lm = LinearModel(nepochs = nepochs)
    r = range(10)
    df_X_train = dt.Frame(r)
    df_y_train = dt.Frame(r)
    df_X_validate = dt.Frame(range(-10, 10))
    df_y_validate = df_X_validate
    res = lm.fit(df_X_train, df_y_train,
                 df_X_validate, df_y_validate,
                 nepochs_validation = nepochs_validation,
                 validation_average_niterations = validation_average_niterations)
    p = lm.predict(df_X_train)
    delta = [abs(i - j) for i, j in zip(p.to_list()[0], list(r))]
    assert res.epoch < nepochs
    assert res.loss < epsilon
    assert int(res.epoch) % nepochs_validation == 0
    assert max(delta) < epsilon



#-------------------------------------------------------------------------------
# Test pickling
#-------------------------------------------------------------------------------

def test_linearmodel_pickling_empty_model():
    lm_pickled = pickle.dumps(LinearModel())
    lm_unpickled = pickle.loads(lm_pickled)
    assert lm_unpickled.model is None
    assert lm_unpickled.params == LinearModel().params


def test_linearmodel_pickling_empty_model_reuse():
    lm = LinearModel()
    lm_pickled = pickle.dumps(lm)
    lm_unpickled = pickle.loads(lm_pickled)
    df_train = dt.Frame({"id" : [1]})
    df_target = dt.Frame([1.0])
    lm_unpickled.fit(df_train, df_target)
    assert_equals(lm_unpickled.model, dt.Frame([lm.eta0, lm.eta0]/stype.float32))


def test_linearmodel_pickling_binomial():
    nrows = 10
    lm = LinearModel()
    df_train = dt.Frame(range(nrows), names = ["f1"])
    df_target = dt.Frame([True] * nrows)
    lm.fit(df_train, df_target)

    lm_pickled = pickle.dumps(lm)
    lm_unpickled = pickle.loads(lm_pickled)
    frame_integrity_check(lm_unpickled.model)
    assert_equals(lm.model, lm_unpickled.model)
    assert lm.params == lm_unpickled.params
    assert_equals(lm.labels, lm_unpickled.labels)

    # Predict
    target = lm.predict(df_train)
    target_unpickled = lm_unpickled.predict(df_train)
    assert_equals(lm.model, lm_unpickled.model)
    assert_equals(target, target_unpickled)

    # Fit and predict
    lm_unpickled.fit(df_train, df_target)
    target_unpickled = lm_unpickled.predict(df_train)
    lm.fit(df_train, df_target)
    target = lm.predict(df_train)
    assert_equals(lm.model, lm_unpickled.model)
    assert_equals(target, target_unpickled)

