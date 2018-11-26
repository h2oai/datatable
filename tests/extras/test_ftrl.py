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

#-------------------------------------------------------------------------------
# Define namedtuple of test parameters
#-------------------------------------------------------------------------------
Params = collections.namedtuple("Params",["a", "b", "l1", "l2", "d", "n_epochs",
                                           "inter"])

fp = Params(a = 1, b = 2, l1 = 3, l2 = 4, d = 5, n_epochs = 6, 
                inter = True)


#-------------------------------------------------------------------------------
# Test wrong parameter types, names and combination in constructor
#-------------------------------------------------------------------------------
   
def test_ftrl_wrong_a():
    with pytest.raises(TypeError) as e:
        ft = core.Ftrl(a = "1.0")
    assert ("Expected a float, instead got <class 'str'>" ==
            str(e.value))
    
    
def test_ftrl_wrong_b():
    with pytest.raises(TypeError) as e:
        ft = core.Ftrl(b = "1.0")
    assert ("Expected a float, instead got <class 'str'>" ==
            str(e.value))
    
    
def test_ftrl_wrong_l1():
    with pytest.raises(TypeError) as e:
        ft = core.Ftrl(l1 = "1.0")
    assert ("Expected a float, instead got <class 'str'>" ==
            str(e.value))
    
    
def test_ftrl_wrong_l2():
    with pytest.raises(TypeError) as e:
        ft = core.Ftrl(l2 = "1.0")
    assert ("Expected a float, instead got <class 'str'>" ==
            str(e.value))
    
    
def test_ftrl_wrong_d():
    with pytest.raises(TypeError) as e:
        ft = core.Ftrl(d = 1000000.0)
    assert ("Expected an integer, instead got <class 'float'>" ==
            str(e.value))


def test_ftrl_wrong_n_epochs():
    with pytest.raises(TypeError) as e:
        ft = core.Ftrl(n_epochs = 10.0)
    assert ("Expected an integer, instead got <class 'float'>" ==
            str(e.value))
    
    
def test_ftrl_wrong_inter():
    with pytest.raises(TypeError) as e:
        ft = core.Ftrl(inter = 2)
    assert ("Expected a boolean, instead got <class 'int'>" ==
            str(e.value))
    
    
def test_ftrl_create_wrong_combination():
    with pytest.raises(TypeError) as e:
        ft = core.Ftrl(params=fp, a = fp.a)
    assert ("You can either pass all the parameters with `params` or  any of the "
            "individual parameters with `a`, `b`, `l1`, `l2`, `d`,`n_epchs` or "
            "`inter` to Ftrl constructor, but not both at the same time" ==
            str(e.value))


def test_ftrl_unknown_arg():
    with pytest.raises(TypeError) as e:
        ft = core.Ftrl(c = 1.0)
    assert ("Ftrl() constructor got an unexpected keyword argument `c`" ==
            str(e.value))    

   
#-------------------------------------------------------------------------------
# Test creation of Ftrl object
#-------------------------------------------------------------------------------

def test_ftrl_create_default():
    ft = core.Ftrl()
    assert ft.params == ft.default_params
    
    
def test_ftrl_create_params():
    ft = core.Ftrl(params=fp)
    assert ft.params == fp
    
    
def test_ftrl_create_individual():
    ft = core.Ftrl(a = fp.a, b = fp.b, l1 = fp.l1, l2 = fp.l2, d = fp.d, 
                   n_epochs = fp.n_epochs, inter = fp.inter)
    assert ft.params == (fp.a, fp.b, fp.l1, fp.l2, fp.d, fp.n_epochs, fp.inter)
    
    
#-------------------------------------------------------------------------------
# Test getters, setters and reset methods for FTRL parameters
#-------------------------------------------------------------------------------

def test_ftrl_get_params_individual():
    ft = core.Ftrl(params = fp)
    assert ft.params == fp
    assert (ft.a, ft.b, ft.l1, ft.l2, ft.d, ft.n_epochs, ft.inter) == fp
    

def test_ftrl_set_params():
    ft = core.Ftrl()
    ft.params = fp
    assert ft.params == fp
    
    
def test_ftrl_set_individual():
    ft = core.Ftrl()
    ft.a = fp.a
    ft.b = fp.b
    ft.l1 = fp.l1
    ft.l2 = fp.l2
    ft.d = fp.d
    ft.n_epochs = fp.n_epochs
    ft.inter = fp.inter
    assert ft.params == fp
    
    
def test_ftrl_reset_params():
    ft = core.Ftrl(params = fp)
    assert ft.params != ft.default_params
    ft.reset_params()
    assert ft.params == ft.default_params
    
    
#-------------------------------------------------------------------------------
# Test getters, setters and reset methods for FTRL model
#-------------------------------------------------------------------------------

def test_ftrl_model_none():
    ft = core.Ftrl()
    assert ft.model == None
    

def test_ftrl_get_set_reset_model():
    ft = core.Ftrl(params = fp)
    model = dt.Frame({"z" : [random.random() for i in range(fp.d)],
                      "n" : [random.random() for i in range(fp.d)]})
    ft.model = model
    assert_equals(ft.model, model)
    ft.reset()
    assert ft.model == None
    
    
#-------------------------------------------------------------------------------
# Test wrong training frame
#-------------------------------------------------------------------------------

def test_ftrl_fit_empty():
    ft = core.Ftrl()
    df_train = dt.Frame()
    with pytest.raises(ValueError) as e:
        ft.fit(df_train)
    assert ("Cannot train a model on an empty frame" ==
            str(e.value))
    

def test_ftrl_fit_one_column():
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
    assert ("Can only predict on a frame that has %d column(s), i.e. the same "
            "number of features as was used for model training" 
            % (df_train.ncols - 1) == str(e.value))
