#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2021 H2O.ai
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
          ["eta", "lambda1", "lambda2", "nepochs", "double_precision",
           "negative_class", "interactions", "model_type"]
         )

params_test = Params(eta = 1,
                 lambda1 = 2,
                 lambda2 = 3,
                 nepochs = 4.0,
                 double_precision = True,
                 negative_class = True,
                 interactions = (("C0",),),
                 model_type = 'binomial')


params_default = Params(eta = 0.005,
                        lambda1 = 0,
                        lambda2 = 0,
                        nepochs = 1,
                        double_precision = False,
                        negative_class = False,
                        interactions = None,
                        model_type = 'auto')

epsilon = 0.01 # Accuracy required for some tests


#-------------------------------------------------------------------------------
# Test wrong parameter types, names and their combinations in constructor
#-------------------------------------------------------------------------------

def test_linearmodel_construct_wrong_eta_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(eta = "1.0"))
    assert ("Argument eta in LinearModel() constructor should be a float, instead "
            "got <class 'str'>" == str(e.value))


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


def test_linearmodel_construct_wrong_interactions_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(interactions = "C0"))
    assert ("Argument interactions in LinearModel() constructor should be a list or a tuple, "
            "instead got: <class 'str'>" == str(e.value))


def test_linearmodel_construct_wrong_interaction_type():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(interactions = ["C0"]))
    assert ("Argument interactions in LinearModel() constructor should be a list or a tuple "
            "of lists or tuples, instead encountered: 'C0'" == str(e.value))


def test_linearmodel_construct_wrong_interactions_from_itertools():
    import itertools
    with pytest.raises(TypeError) as e:
        noop(LinearModel(interactions = itertools.combinations(["C0", "C1"], 2)))
    assert ("Argument interactions in LinearModel() constructor should be a list or a tuple, "
            "instead got: <class 'itertools.combinations'>"
            == str(e.value))


def test_linearmodel_construct_wrong_combination():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(params=params_test, eta = params_test.eta))
    assert ("You can either pass all the parameters with params or any of "
            "the individual parameters with eta, lambda1, "
            "lambda2, nepochs, double_precision, "
            "negative_class, interactions or model_type to LinearModel constructor, "
            "but not both at the same time"
            == str(e.value))


def test_linearmodel_construct_unknown_arg():
    with pytest.raises(TypeError) as e:
        noop(LinearModel(c = 1.0))
    assert ("LinearModel() constructor got an unexpected keyword argument c" ==
            str(e.value))


def test_linearmodel_construct_wrong_params_type():
    params = params_test._replace(eta = "1.0")
    with pytest.raises(TypeError) as e:
        LinearModel(params)
    assert ("LinearModelParams.eta should be a float, instead got <class 'str'>"
            == str(e.value))


def test_linearmodel_construct_wrong_params_name():
    WrongParams = collections.namedtuple("WrongParams",["eta", "lambda1"])
    wrong_params = WrongParams(eta = 1, lambda1 = 0.01)
    with pytest.raises(AttributeError) as e:
        LinearModel(wrong_params)
    assert ("'WrongParams' object has no attribute 'double_precision'"
            == str(e.value))


#-------------------------------------------------------------------------------
# Test wrong parameter values in constructor
#-------------------------------------------------------------------------------

def test_linearmodel_construct_wrong_eta_value():
    with pytest.raises(ValueError) as e:
        noop(LinearModel(eta = 0.0))
    assert ("Argument eta in LinearModel() constructor should be positive: 0.0"
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


#-------------------------------------------------------------------------------
# Test creation of LinearModel object
#-------------------------------------------------------------------------------

def test_linearmodel_create_default():
    lm = LinearModel()
    assert lm.params == params_default
    assert lm.model_type_trained == "none"
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
    lm = LinearModel(eta = params_test.eta,
              lambda1 = params_test.lambda1, lambda2 = params_test.lambda2,
              nepochs = params_test.nepochs,
              double_precision = params_test.double_precision,
              negative_class = params_test.negative_class,
              interactions = params_test.interactions,
              model_type = params_test.model_type)
    assert lm.params == (params_test.eta,
                         params_test.lambda1, params_test.lambda2,
                         params_test.nepochs,
                         params_test.double_precision, params_test.negative_class,
                         params_test.interactions, params_test.model_type)


#-------------------------------------------------------------------------------
# Test getters and setters for valid LineamModel parameters
#-------------------------------------------------------------------------------

def test_linearmodel_get_params():
    lm = LinearModel(params_test)
    params = lm.params
    assert params == params_test
    assert (lm.eta, lm.lambda1, lm.lambda2,
           lm.nepochs, lm.double_precision, lm.negative_class, lm.interactions,
           lm.model_type) == params_test
    assert (params.eta, params.lambda1, params.lambda2,
           params.nepochs, params.double_precision,
           params.negative_class, params.interactions, params.model_type) == params_test


def test_linearmodel_set_individual():
    lm = LinearModel(double_precision = params_test.double_precision)
    lm.eta = params_test.eta
    lm.lambda1 = params_test.lambda1
    lm.lambda2 = params_test.lambda2
    lm.nepochs = params_test.nepochs
    lm.negative_class = params_test.negative_class
    lm.interactions = params_test.interactions
    lm.model_type = params_test.model_type


def test_linearmodel_set_individual_almer_params():
    lm = LinearModel()
    params = lm.params
    lm.eta = params_test.eta
    params_new = lm.params
    assert params == LinearModel().params
    assert (params_new.eta, params_new.lambda1, params_new.lambda2,
           params_new.nepochs,
           params_new.double_precision, params_new.negative_class,
           params_new.interactions, params_new.model_type) == params_new
    assert (lm.eta, lm.lambda1, lm.lambda2,
           lm.nepochs,
           lm.double_precision, lm.negative_class,
           lm.interactions, lm.model_type) == params_new

#-------------------------------------------------------------------------------
# Test getters and setters for wrong types / names of LinearModel parameters
#-------------------------------------------------------------------------------

def test_linearmodel_set_wrong_eta_type():
    lm = LinearModel()
    with pytest.raises(TypeError) as e:
        lm.eta = "0.0"
    assert (".eta should be a float, instead got <class 'str'>" == str(e.value))


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


def test_linearmodel_set_wrong_interactions_type():
    lm = LinearModel()
    with pytest.raises(TypeError) as e:
        lm.interactions = True
    assert (".interactions should be a list or a tuple, instead got: <class 'bool'>"
            == str(e.value))


def test_linearmodel_set_wrong_interactions_empty():
    lm = LinearModel()
    with pytest.raises(ValueError) as e:
        lm.interactions = [["C0"], []]
    assert ("Interaction cannot have zero features, encountered: []" == str(e.value))


def test_linearmodel_set_wrong_interactions_not_list():
    lm = LinearModel()
    with pytest.raises(TypeError) as e:
        lm.interactions = ["a", [1, 2]]
    assert (".interactions should be a list or a tuple of lists or tuples, "
           "instead encountered: 'a'" == str(e.value))


def test_linearmodel_set_wrong_interactions_not_string():
    lm = LinearModel()
    with pytest.raises(TypeError) as e:
        lm.interactions = [["a", "b"], [1, 2]]
    assert ("Interaction features should be strings, instead "
            "encountered: 1" == str(e.value))


#-------------------------------------------------------------------------------
# Test getters and setters for wrong values of individual LinearModel parameters
#-------------------------------------------------------------------------------

@pytest.mark.parametrize('value, message',
                         [[0.0, ".eta should be positive: 0.0"],
                          [None, ".eta should be positive: None"],
                          [math.nan, ".eta should be positive: nan"],
                          [math.inf, ".eta should be finite: inf"]
                         ])
def test_linearmodel_set_bad_eta_value(value, message):
    lm = LinearModel()
    with pytest.raises(ValueError) as e:
        lm.eta = value
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
    df_target = dt.Frame([3, "point", None, None, 14, 15, {92}, "6"])
    with pytest.raises(TypeError) as e:
        lm.fit(df_train, df_target)
    assert ("Target column type obj64 is not supported" ==
            str(e.value))


#-------------------------------------------------------------------------------
# Test wrong parameters for `fit()` and `predict()` methods
#-------------------------------------------------------------------------------

def test_linearmodel_model_untrained():
    lm = LinearModel()
    assert lm.model == None


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
    assert df_target == None


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
    assert ("Can only predict on a frame that has 1 column, i.e. has the "
            "same number of features as was used for model training"
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

def test_linearmodel_fit_none():
    nrows = 10
    lm = LinearModel(model_type = "binomial")
    df_train = dt.Frame(range(nrows))
    df_target = dt.Frame([None] * nrows)
    res = lm.fit(df_train, df_target)
    assert lm.model is None
    assert not lm.labels
    assert lm.model_type == "binomial"
    assert lm.model_type_trained == "none"
    assert res.epoch == 0.0
    assert res.loss is None


def test_linearmodel_fit_unique():
    nrows = 10
    lm = LinearModel()
    df_train = dt.Frame(range(nrows))
    df_target = dt.Frame([True] * nrows)
    lm.fit(df_train, df_target)
    assert_equals(
        lm.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert lm.model_type_trained == "binomial"


def test_linearmodel_fit_unique_ignore_none():
    nrows = 10
    lm = LinearModel()
    df_train = dt.Frame(range(2 * nrows))
    df_target = dt.Frame([True] * nrows + [None] * nrows)
    lm.fit(df_train, df_target)
    assert_equals(
        lm.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert lm.model_type_trained == "binomial"


def test_linearmodel_fit_predict_bool():
    lm = LinearModel(eta = 0.1, nepochs = 10000)
    df_train = dt.Frame([[True, False]])
    df_target = dt.Frame([[True, False]])
    lm.fit(df_train, df_target)
    df_target = lm.predict(df_train[:,0])
    assert_equals(
        lm.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert lm.model_type_trained == "binomial"
    assert df_target[0, 1] <= 1
    assert df_target[0, 1] >= 1 - epsilon
    assert df_target[1, 1] >= 0
    assert df_target[1, 1] < epsilon


def test_linearmodel_fit_predict_int():
    lm = LinearModel(eta = 0.1, nepochs = 10000)
    df_train = dt.Frame([[0, 1]])
    df_target = dt.Frame([[True, False]])
    lm.fit(df_train, df_target)
    df_target = lm.predict(df_train[:,0])
    assert lm.model_type_trained == "binomial"
    assert df_target[0, 1] <= 1
    assert df_target[0, 1] >= 1 - epsilon
    assert df_target[1, 1] >= 0
    assert df_target[1, 1] < epsilon


def test_linearmodel_fit_predict_float():
    lm = LinearModel(eta = 0.1, nepochs = 10000)
    df_train = dt.Frame([[0.0, 1.0, math.inf]])
    df_target = dt.Frame([[True, False, False]])
    lm.fit(df_train, df_target)
    df_target = lm.predict(df_train[:,0])
    assert lm.model_type_trained == "binomial"
    assert df_target[0, 1] <= 1
    assert df_target[0, 1] >= 1 - epsilon
    assert df_target[1, 1] >= 0
    assert df_target[1, 1] < epsilon


# def test_linearmodel_fit_predict_string():
#     lm = LinearModel(eta = 0.1, nepochs = 10000)
#     df_train = dt.Frame([["Monday", None, "", "Tuesday"]])
#     df_target = dt.Frame([[True, False, False, True]])
#     lm.fit(df_train, df_target)
#     df_target = lm.predict(df_train[:,0])
#     assert lm.model_type_trained == "binomial"
#     assert df_target[0, 1] <= 1
#     assert df_target[0, 1] >= 1 - epsilon
#     assert df_target[1, 1] >= 0
#     assert df_target[1, 1] < epsilon


@pytest.mark.parametrize('target',
                         [[True, False],
                         ["yes", "no"],
                         [20, 10],
                         [0.5, -0.5]])
def test_linearmodel_fit_predict_bool_binomial(target):
    lm = LinearModel(eta = 0.1, nepochs = 10000, model_type = "binomial")
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
    assert lm.model_type_trained == "binomial"
    assert df_res[0, 1] <= 1
    assert df_res[0, 1] >= 1 - epsilon
    assert df_res[1, 1] >= 0
    assert df_res[1, 1] < epsilon
    assert df_res[0, 0] >= 0
    assert df_res[0, 0] < epsilon
    assert df_res[1, 0] <= 1
    assert df_res[1, 0] >= 1 - epsilon


def test_linearmodel_fit_predict_view():
    nrows = 100
    lm = LinearModel()
    df_train = dt.Frame(random.sample(range(nrows), nrows))
    df_target = dt.Frame([bool(random.getrandbits(1)) for _ in range(nrows)])
    rows = range(nrows//2, nrows)

    # Train model and predict on a view
    lm.fit(df_train[rows,:], df_target[rows,:])
    predictions = lm.predict(df_train[rows,:])
    model = lm.model

    # Train model and predict on a frame
    lm.reset()
    df_train_range = df_train[rows,:]
    df_target_range = df_target[rows,:]
    df_train_range.materialize()
    df_target_range.materialize()
    lm.fit(df_train_range, df_target_range)
    predictions_range = lm.predict(df_train_range)

    assert_equals(
        lm.labels,
        dt.Frame(label=[False, True], id=[0, 1], stypes={"id" : stype.int32})
    )
    assert lm.model_type_trained == "binomial"
    assert_equals(model, lm.model)
    assert_equals(predictions, predictions_range)


@pytest.mark.parametrize('parameter, value',
                         [("interactions", [["C0", "C0"]]),
                         ("negative_class", True)])
def test_linearmodel_disable_setters_after_fit(parameter, value):
    nrows = 10
    lm = LinearModel()
    df_train = dt.Frame(range(nrows))
    df_target = dt.Frame([True] * nrows)
    lm.fit(df_train, df_target)

    assert lm.model_type_trained == "binomial"
    with pytest.raises(ValueError) as e:
        setattr(lm, parameter, value)
    assert ("Cannot change ."+parameter+" for a trained model, "
            "reset this model or create a new one"
            == str(e.value))
    lm.reset()
    setattr(lm, parameter, value)


# def test_linearmodel_fit_predict_binomial_online_1_1():
#     lm = LinearModel(eta = 0.1, nepochs = 10000, model_type = "binomial")
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
#     assert lm.model_type_trained == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon

#     p = lm.predict(df_train_even)
#     p_dict = p.to_dict()
#     delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 1, 1, 1])]
#     delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 0, 0, 0])]
#     assert lm.model_type_trained == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon


# def test_linearmodel_fit_predict_binomial_online_1_2():
#     lm = LinearModel(eta = 0.1, nepochs = 10000, model_type = "binomial")
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
#     assert lm.model_type_trained == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon

#     p = lm.predict(df_train_even_odd)
#     p_dict = p.to_dict()
#     delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 0, 1, 0])]
#     delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 1, 0, 1])]
#     assert lm.model_type_trained == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon


# def test_linearmodel_fit_predict_binomial_online_2_1():
#     lm = LinearModel(eta = 0.1, nepochs = 10000, model_type = "binomial")
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
#     assert lm.model_type_trained == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon

#     p = lm.predict(df_train_even_odd)
#     p_dict = p.to_dict()
#     delta_even = [abs(i - j) for i, j in zip(p_dict["even"], [1, 0, 1, 0])]
#     delta_odd = [abs(i - j) for i, j in zip(p_dict["odd"], [0, 1, 0, 1])]
#     assert lm.model_type_trained == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon


# def test_linearmodel_fit_predict_binomial_online_2_2():
#     lm = LinearModel(eta = 0.1, nepochs = 10000, model_type = "binomial")
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
#     assert lm.model_type_trained == "binomial"
#     assert max(delta_odd) < epsilon
#     assert max(delta_even) < epsilon


#-------------------------------------------------------------------------------
# Test multinomial regression
#-------------------------------------------------------------------------------

def test_linearmodel_fit_multinomial_none():
    nrows = 10
    lm = LinearModel(model_type = "multinomial")
    df_train = dt.Frame(range(nrows))
    df_target = dt.Frame([None] * nrows)
    res = lm.fit(df_train, df_target)
    assert not lm.labels
    assert lm.model_type == "multinomial"
    assert lm.model_type_trained == "none"
    assert res.epoch == 0.0
    assert res.loss is None


def test_linearmodel_fit_predict_multinomial_vs_binomial():
    target_names = ["cat", "dog"]
    lm_binomial = LinearModel(nepochs = 1)
    df_train_binomial = dt.Frame(range(10))
    df_target_binomial = dt.Frame({"target" : [False, True] * 5})
    lm_binomial.fit(df_train_binomial, df_target_binomial)
    p_binomial = lm_binomial.predict(df_train_binomial)
    p_binomial.names = target_names

    lm_multinomial = LinearModel(nepochs = 1, model_type = "multinomial")
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
    assert lm_binomial.model_type_trained == "binomial"
    assert lm_multinomial.model_type_trained == "multinomial"
    assert_equals(lm_binomial.model, multinomial_model)
    assert_equals(p_binomial[:, 0], p_multinomial[:, 0])


# @pytest.mark.parametrize('negative_class', [False, True])
# def test_linearmodel_fit_predict_multinomial(negative_class):
#     negative_class_label = ["_negative_class"] if negative_class else []
#     nepochs = 1000
#     lm = LinearModel(eta = 0.2, nepochs = nepochs, double_precision = True)
#     lm.negative_class = negative_class
#     labels = negative_class_label + ["blue", "green", "red"]

#     df_train = dt.Frame(["cucumber", None, "shilm", "sky", "day", "orange", "ocean"])
#     df_target = dt.Frame(["green", "red", "red", "blue", "green", None, "blue"])
#     lm.fit(df_train, df_target)
#     frame_integrity_check(lm.model)
#     p = lm.predict(df_train)

#     frame_integrity_check(p)
#     p_none = 1 / p.ncols
#     p_dict = p.to_dict()
#     p_list = p.to_list()
#     sum_p =[sum(row) for row in zip(*p_list)]
#     delta_sum = [abs(i - j) for i, j in zip(sum_p, [1] * 5)]
#     delta_red =   [abs(i - j) for i, j in
#                    zip(p_dict["red"], [0, 1, 1, 0, 0, p_none, 0])]
#     delta_green = [abs(i - j) for i, j in
#                    zip(p_dict["green"], [1, 0, 0, 0, 1, p_none, 0])]
#     delta_blue =  [abs(i - j) for i, j in
#                    zip(p_dict["blue"], [0, 0, 0, 1, 0, p_none, 1])]

#     ids = [0, 3, 1, 2] if negative_class else [2, 0, 1]
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
#     )
#     assert lm.model_type_trained == "multinomial"
#     assert max(delta_sum)   < 1e-12
#     assert max(delta_red)   < epsilon
#     assert max(delta_green) < epsilon
#     assert max(delta_blue)  < epsilon
#     assert list(p.names) == labels


# @pytest.mark.parametrize('negative_class', [False, True])
# def test_linearmodel_fit_predict_multinomial_online(negative_class):
#     lm = LinearModel(eta = 0.2, nepochs = 1000, double_precision = True)
#     lm.negative_class = negative_class
#     negative_class_label = ["_negative_class"] if negative_class else []

#     # Show only 1 label to the model
#     df_train = dt.Frame(["cucumber"])
#     df_target = dt.Frame(["green"])
#     lm.fit(df_train, df_target)

#     labels = negative_class_label + ["green"]
#     ids = list(range(len(labels)))
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
#     )
#     assert lm.model_type_trained == "multinomial"
#     assert lm.model.shape == (lm.nbins, 2 * lm.labels.nrows)

#     # Also do pickling unpickling in the middle.
#     lm_pickled = pickle.dumps(lm)
#     lm = pickle.loads(lm_pickled)

#     # Show one more
#     df_train = dt.Frame(["cucumber", None])
#     df_target = dt.Frame(["green", "red"])
#     lm.fit(df_train, df_target)

#     labels += ["red"]
#     ids += [len(ids)]
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
#     )
#     assert lm.model_type_trained == "multinomial"
#     assert lm.model.shape == (lm.nbins, 2 * lm.labels.nrows)

#     # And one more
#     df_train = dt.Frame(["cucumber", None, "shilm", "sky", "day", "orange", "ocean"])
#     df_target = dt.Frame(["green", "red", "red", "blue", "green", None, "blue"])
#     lm.fit(df_train, df_target)

#     labels.insert(negative_class, "blue")
#     ids.insert(negative_class, len(ids))
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
#     )
#     assert lm.model_type_trained == "multinomial"
#     assert lm.model.shape == (lm.nbins, 2 * lm.labels.nrows)

#     # Do not add any new labels
#     df_train = dt.Frame(["cucumber", None, "shilm", "sky", "day", "orange", "ocean"])
#     df_target = dt.Frame(["green", "red", "red", "blue", "green", None, "blue"])

#     lm.fit(df_train, df_target)
#     assert_equals(
#         lm.labels,
#         dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
#     )
#     assert lm.model_type_trained == "multinomial"
#     assert lm.model.shape == (lm.nbins, 2 * lm.labels.nrows)

#     # Test predictions
#     p = lm.predict(df_train)
#     frame_integrity_check(p)
#     p_none = 1 / p.ncols
#     p_dict = p.to_dict()
#     p_list = p.to_list()
#     sum_p =[sum(row) for row in zip(*p_list)]
#     delta_sum = [abs(i - j) for i, j in zip(sum_p, [1] * 5)]
#     delta_red =   [abs(i - j) for i, j in
#                    zip(p_dict["red"], [0, 1, 1, 0, 0, p_none, 0])]
#     delta_green = [abs(i - j) for i, j in
#                    zip(p_dict["green"], [1, 0, 0, 0, 1, p_none, 0])]
#     delta_blue =  [abs(i - j) for i, j in
#                    zip(p_dict["blue"], [0, 0, 0, 1, 0, p_none, 1])]

#     assert_equals(
#         lm.labels,
#         dt.Frame(label=labels, id=ids, stypes={"id": dt.int32})
#     )
#     assert lm.model_type_trained == "multinomial"
#     assert max(delta_sum)   < 1e-12
#     assert max(delta_red)   < epsilon
#     assert max(delta_green) < epsilon
#     assert max(delta_blue)  < epsilon
#     assert list(p.names) == negative_class_label + ["blue", "green", "red"]


#-------------------------------------------------------------------------------
# Test regression for numerical targets
#-------------------------------------------------------------------------------

# FIXME: we shouldn't start numeric regression when get only None's
def test_linearmodel_regression_fit_none():
    nrows = 10
    lm = LinearModel(model_type = "regression")
    df_train = dt.Frame(range(nrows))
    df_target = dt.Frame([None] * nrows)
    res = lm.fit(df_train, df_target)
    assert_equals(
        lm.labels,
        dt.Frame(label=["C0"], id=[0], stypes={"id": dt.int32})
    )
    assert lm.model_type == "regression"
    assert lm.model_type_trained == "regression"
    assert res.epoch == 1.0
    assert res.loss is None


def test_linearmodel_regression_fit_simple_zero():
    lm = LinearModel(nepochs = 1, double_precision = True)
    df_train = dt.Frame([0])
    df_target = dt.Frame([0])
    lm.fit(df_train, df_target)
    assert lm.model_type_trained == "regression"
    assert_equals(lm.model, dt.Frame([0.0, 0.0]))


def test_linearmodel_regression_fit_simple_one():
    lm = LinearModel(nepochs = 1, double_precision = True)
    df_train = dt.Frame([1])
    df_target = dt.Frame([1])
    lm.fit(df_train, df_target)
    assert lm.model_type_trained == "regression"
    assert_equals(lm.model, dt.Frame([0.01, 0.0099]))


def test_linearmodel_regression_fit_predict():
    lm = LinearModel(nepochs = 1000)
    r = list(range(9))
    df_train = dt.Frame(r + [0])
    df_target = dt.Frame(r + [math.inf])
    lm.fit(df_train, df_target)
    p = lm.predict(df_train)
    assert_equals(
        lm.labels,
        dt.Frame(label=["C0"], id=[0], stypes={"id": dt.int32})
    )
    delta = [abs(i - j) for i, j in zip(p.to_list()[0], r + [0])]
    assert lm.labels[:, 0].to_list() == [["C0"]]
    assert lm.model_type_trained == "regression"
    assert max(delta) < epsilon


#-------------------------------------------------------------------------------
# Test early stopping
#-------------------------------------------------------------------------------

def test_linearmodel_wrong_validation_target_type():
    nepochs = 1234
    nepochs_validation = 56
    nrows = 78
    lm = LinearModel(eta = 0.5, nepochs = nepochs)
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


def test_linearmodel_wrong_validation_parameters():
    nepochs = 1234
    nepochs_validation = 56
    nrows = 78
    lm = LinearModel(eta = 0.5, nepochs = nepochs)
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
def test_linearmodel_no_validation_set(double_precision_value):
    nepochs = 1234
    nrows = 56
    lm = LinearModel(eta = 0.5, nepochs = nepochs,
                     double_precision = double_precision_value)
    r = range(nrows)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    res = lm.fit(df_X, df_y)
    assert res.epoch == nepochs
    assert res.loss is None


def test_linearmodel_no_early_stopping():
    nepochs = 100
    nepochs_validation = 10
    lm = LinearModel(nepochs = nepochs)
    r = range(10)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r)
    res = lm.fit(df_X, df_y, df_X, df_y,
                 nepochs_validation = nepochs_validation)
    assert res.epoch == nepochs
    assert math.isnan(res.loss) == False


def test_linearmodel_wrong_validation_stypes():
    lm = LinearModel(eta = 0.5)
    r = range(10)
    df_X = dt.Frame(r)
    df_y = dt.Frame(r) # int32 stype
    df_X_val = dt.Frame(["1"] * 10)
    df_y_val = dt.Frame(list(r)) # int8 stype

    msg = r"Training and validation frames must have identical column ltypes, " \
           "instead for a column C0, got ltypes: int and str"
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


# def test_linearmodel_early_stopping_multinomial():
#     nepochs = 2000
#     lm = LinearModel(eta = 0.2, nepochs = nepochs, double_precision = True)
#     labels = ["blue", "green", "red"]

#     df_train = dt.Frame(["cucumber", None, "shilm", "sky", "day", "orange",
#                          "ocean"])
#     df_target = dt.Frame(["green", "red", "red", "blue", "green", None,
#                           "blue"])
#     res = lm.fit(df_train, df_target, df_train[:4, :], df_target[:4, :],
#                  nepochs_validation = 1, validation_error = 1e-3)
#     frame_integrity_check(lm.model)
#     p = lm.predict(df_train)
#     frame_integrity_check(p)
#     p_none = 1/p.ncols
#     p_dict = p.to_dict()
#     p_list = p.to_list()
#     sum_p =[sum(row) for row in zip(*p_list)]
#     delta_sum = [abs(i - j) for i, j in zip(sum_p, [1] * 5)]
#     delta_red =   [abs(i - j) for i, j in
#                    zip(p_dict["red"], [0, 1, 1, 0, 0, p_none, 0])]
#     delta_green = [abs(i - j) for i, j in
#                    zip(p_dict["green"], [1, 0, 0, 0, 1, p_none, 0])]
#     delta_blue =  [abs(i - j) for i, j in
#                    zip(p_dict["blue"], [0, 0, 0, 1, 0, p_none, 1])]

#     assert res.epoch < nepochs
#     assert res.loss < 0.1
#     assert max(delta_sum)   < 1e-6
#     assert max(delta_red)   < epsilon
#     assert max(delta_green) < epsilon
#     assert max(delta_blue)  < epsilon
#     assert list(p.names) == labels


#-------------------------------------------------------------------------------
# Test feature importance
#-------------------------------------------------------------------------------

# def test_linearmodel_feature_importances():
#     nrows = 10**4
#     feature_names = ['unique', 'boolean', 'mod100']
#     lm = LinearModel()
#     df_train = dt.Frame([range(nrows),
#                          [i % 2 for i in range(nrows)],
#                          [i % 100 for i in range(nrows)]
#                         ], names = feature_names)
#     df_target = dt.Frame([False, True] * (nrows // 2))
#     lm.fit(df_train, df_target)
#     fi = lm.feature_importances
#     assert fi[1].min1() >= 0
#     assert math.isclose(fi[1].max1(), 1, abs_tol = 1e-7)
#     assert fi.stypes == (stype.str32, stype.float32)
#     assert fi.names == ("feature_name", "feature_importance")
#     assert fi[0].to_list() == [feature_names]
#     assert fi[0, 1] < fi[2, 1]
#     assert fi[2, 1] < fi[1, 1]


def test_linearmodel_feature_importances_none():
    lm = LinearModel()
    assert lm.feature_importances == None


def test_linearmodel_feature_importances_empty():
    lm = LinearModel(nepochs = 0, double_precision = True)
    lm.fit(dt.Frame(range(10)), dt.Frame(range(10)))
    DT = dt.Frame(
      [["C0"], [0.0]],
      names = ("feature_name", "feature_importance")
    )
    assert_equals(lm.feature_importances, DT)


def test_linearmodel_fi_shallowcopy():
    import copy
    nrows = 10
    lm = LinearModel(params_test)
    df_train = dt.Frame(random.sample(range(nrows), nrows))
    df_target = dt.Frame([bool(random.getrandbits(1)) for _ in range(nrows)])
    lm.fit(df_train, df_target)
    fi1 = lm.feature_importances
    fi1 = lm.feature_importances
    assert fi1[1].min1() >= 0
    assert math.isclose(fi1[1].max1(), 1, abs_tol = 1e-7)

    fi2 = copy.deepcopy(lm.feature_importances)
    lm.reset()
    assert lm.feature_importances is None
    assert_equals(fi1, fi2)


#-------------------------------------------------------------------------------
# Test feature interactions
#-------------------------------------------------------------------------------

def test_linearmodel_interactions_wrong_features():
    nrows = 10**4
    feature_names = ['unique', 'boolean', 'mod100']
    feature_interactions = [["unique", "boolean"],["unique", "mod1000"]]
    lm = LinearModel()
    lm.interactions = feature_interactions
    df_train = dt.Frame([range(nrows),
                         [i % 2 for i in range(nrows)],
                         [i % 100 for i in range(nrows)]
                        ], names = feature_names)
    df_target = dt.Frame([False, True] * (nrows // 2))
    with pytest.raises(ValueError) as e:
        lm.fit(df_train, df_target)
    assert ("Feature mod1000 is used in the interactions, however, column "
            "mod1000 is missing in the training frame" == str(e.value))


@pytest.mark.parametrize("interactions", [
                         [["feature1", "feature2", "feature3"], ["feature3", "feature2"]],
                         [("feature1", "feature2", "feature3"), ("feature3", "feature2")],
                         (["feature1", "feature2", "feature3"], ["feature3", "feature2"]),
                         (("feature1", "feature2", "feature3"), ("feature3", "feature2")),
                        ])
def test_linearmodel_interactions_formats(interactions):
    lm = LinearModel(interactions = interactions)
    df_train = dt.Frame(
                 [range(10), [False, True] * 5, range(1, 100, 10)],
                 names = ["feature1", "feature2", "feature3"]
               )
    df_target = dt.Frame(target = [True, False] * 5)

    lm.fit(df_train, df_target)
    fi = lm.feature_importances
    assert fi[1].min1() >= 0
    assert math.isclose(fi[1].max1(), 1, abs_tol = 1e-7)
    assert (fi[0].to_list() ==
           [["feature1", "feature2", "feature3",
             "feature1:feature2:feature3", "feature3:feature2"]])
    assert lm.interactions == tuple(tuple(interaction) for interaction in interactions)


@pytest.mark.parametrize("struct", [list, tuple])
def test_linearmodel_interactions_from_itertools(struct):
    import itertools
    df_train = dt.Frame(
                 [range(10), [False, True] * 5, range(1, 100, 10)],
                 names = ["feature1", "feature2", "feature3"]
               )
    df_target = dt.Frame(target = [True, False] * 5)
    interactions = struct(itertools.combinations(df_train.names, 2))

    lm = LinearModel(interactions = interactions)
    lm.fit(df_train, df_target)
    fi = lm.feature_importances
    assert fi[1].min1() >= 0
    assert math.isclose(fi[1].max1(), 1, abs_tol = 1e-7)
    assert (fi[0].to_list() ==
           [["feature1", "feature2", "feature3",
             "feature1:feature2", "feature1:feature3", "feature2:feature3"]])


# def test_linearmodel_interactions():
#     nrows = 10**4
#     feature_names = ['unique', 'boolean', 'mod100']
#     feature_interactions = (("unique", "boolean"),
#                             ("unique", "mod100"),
#                             ("boolean", "mod100"),
#                             ("boolean", "boolean", "boolean"))
#     interaction_names = ["unique:boolean", "unique:mod100",
#                          "boolean:mod100", "boolean:boolean:boolean"]
#     lm = LinearModel(interactions = feature_interactions)
#     df_train = dt.Frame([range(nrows),
#                          [i % 2 for i in range(nrows)],
#                          [i % 100 for i in range(nrows)]
#                         ], names = feature_names)
#     df_target = dt.Frame([False, True] * (nrows // 2))
#     lm.fit(df_train, df_target)
#     fi = lm.feature_importances

#     assert fi[1].min1() >= 0
#     assert math.isclose(fi[1].max1(), 1, abs_tol = 1e-7)
#     assert fi.stypes == (stype.str32, stype.float32)
#     assert fi.names == ("feature_name", "feature_importance")
#     assert fi[0].to_list() == [feature_names + interaction_names]
#     assert fi[0, 1] < fi[2, 1]
#     assert fi[2, 1] < fi[1, 1]
#     assert fi[3, 1] < fi[1, 1]
#     assert fi[4, 1] < fi[1, 1]
#     assert fi[5, 1] < fi[1, 1]
#     # Make sure interaction of important features is still an important feature
#     assert fi[0, 1] < fi[6, 1]
#     assert fi[2, 1] < fi[6, 1]
#     assert fi[3, 1] < fi[6, 1]
#     assert fi[4, 1] < fi[6, 1]
#     assert fi[5, 1] < fi[6, 1]

#     # Also check what happens when we reset the model
#     lm.reset()
#     assert lm.interactions == feature_interactions
#     lm.interactions = None
#     lm.fit(df_train, df_target)
#     fi = lm.feature_importances
#     assert fi[1].min1() >= 0
#     assert math.isclose(fi[1].max1(), 1, abs_tol = 1e-7)
#     assert fi[0].to_list() == [feature_names]


#-------------------------------------------------------------------------------
# Test pickling
#-------------------------------------------------------------------------------

def test_linearmodel_pickling_empty_model():
    lm_pickled = pickle.dumps(LinearModel())
    lm_unpickled = pickle.loads(lm_pickled)
    assert lm_unpickled.model == None
    assert lm_unpickled.feature_importances == None
    assert lm_unpickled.params == LinearModel().params


def test_linearmodel_reuse_pickled_empty_model():
    lm_pickled = pickle.dumps(LinearModel())
    lm_unpickled = pickle.loads(lm_pickled)
    df_train = dt.Frame({"id" : [1]})
    df_target = dt.Frame([1.0])
    lm_unpickled.fit(df_train, df_target)
    fi = dt.Frame([["id"], [0.0]/stype.float32])
    fi.names = ["feature_name", "feature_importance"]
    assert_equals(lm_unpickled.model, dt.Frame([0.01, 0.0099]/stype.float32))
    assert_equals(lm_unpickled.feature_importances, fi)


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
    assert (lm_unpickled.feature_importances.names ==
            ('feature_name', 'feature_importance',))
    assert (lm_unpickled.feature_importances.stypes ==
            (stype.str32, stype.float32))
    assert_equals(lm.feature_importances, lm_unpickled.feature_importances)
    assert lm.params == lm_unpickled.params
    assert_equals(lm.labels, lm_unpickled.labels)
    assert lm.colnames == lm_unpickled.colnames

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


# def test_linearmodel_pickling_multinomial():
#     lm = LinearModel(eta = 0.2, nbins = 100, nepochs = 1, double_precision = False)
#     df_train = dt.Frame(["cucumber", None, "shilm", "sky", "day", "orange",
#                          "ocean"])
#     df_target = dt.Frame(["green", "red", "red", "blue", "green", None,
#                           "blue"])
#     lm.interactions = [["C0", "C0"]]
#     lm.fit(df_train, df_target)

#     lm_pickled = pickle.dumps(lm)
#     lm_unpickled = pickle.loads(lm_pickled)
#     frame_integrity_check(lm_unpickled.model)
#     assert lm_unpickled.model.stypes == (stype.float32,) * 6
#     assert_equals(lm.model, lm_unpickled.model)
#     assert (lm_unpickled.feature_importances.names ==
#             ('feature_name', 'feature_importance',))
#     assert (lm_unpickled.feature_importances.stypes ==
#             (stype.str32, stype.float32))
#     assert_equals(lm.feature_importances, lm_unpickled.feature_importances)
#     assert lm.params == lm_unpickled.params
#     assert_equals(lm.labels, lm_unpickled.labels)
#     assert lm.colnames == lm_unpickled.colnames
#     assert lm.interactions == lm_unpickled.interactions

#     # Predict
#     target = lm.predict(df_train)
#     target_unpickled = lm_unpickled.predict(df_train)
#     assert_equals(lm.model, lm_unpickled.model)
#     assert_equals(target, target_unpickled)

#     # Fit and predict
#     lm.fit(df_train, df_target)
#     target = lm.predict(df_train)
#     lm_unpickled.fit(df_train, df_target)
#     target_unpickled = lm_unpickled.predict(df_train)
#     assert_equals(lm.model, lm_unpickled.model)
#     assert_equals(target, target_unpickled)


