#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#-------------------------------------------------------------------------------
import datatable as dt
import pytest



#-------------------------------------------------------------------------------
# split_into_nhot
#-------------------------------------------------------------------------------

def test_split_into_nhot0():
    f0 = dt.Frame(["cat, dog, mouse, peacock, frog",
                   "armadillo, fox, hedgehog",
                   "dog, fox, mouse, cat, peacock",
                   "horse, raccoon, cat, frog, dog"])
    f1 = dt.split_into_nhot(f0)
    f1.internal.check()
    fr = dt.Frame({"cat":       [1, 0, 1, 1],
                   "dog":       [1, 0, 1, 1],
                   "mouse":     [1, 0, 1, 0],
                   "peacock":   [1, 0, 1, 0],
                   "frog":      [1, 0, 0, 1],
                   "armadillo": [0, 1, 0, 0],
                   "fox":       [0, 1, 1, 0],
                   "hedgehog":  [0, 1, 0, 0],
                   "horse":     [0, 0, 0, 1],
                   "raccoon":   [0, 0, 0, 1]})
    assert set(f1.names) == set(fr.names)
    fr = fr[:, f1.names]
    assert f1.names == fr.names
    assert f1.stypes == (dt.stype.bool8, ) * f1.ncols
    assert f1.shape == fr.shape
    assert f1.topython() == fr.topython()


def test_split_into_nhot1():
    f0 = dt.Frame(["  meow  \n",
                   "[ meow]",
                   "['meow' ,purr]",
                   '(\t"meow", \'purr\')',
                   "{purr}"])
    f1 = dt.split_into_nhot(f0)
    f1.internal.check()
    fr = dt.Frame(meow=[1, 1, 1, 1, 0], purr=[0, 0, 1, 1, 1])
    assert set(f1.names) == set(fr.names)
    fr = fr[..., f1.names]
    assert f1.shape == fr.shape == (5, 2)
    assert f1.stypes == (dt.stype.bool8, dt.stype.bool8)
    assert f1.topython() == fr.topython()


def test_split_into_nhot_bad():
    f0 = dt.Frame([[1.25], ["foo"], ["bar"]])
    with pytest.raises(ValueError) as e:
        dt.split_into_nhot(f0)
    assert ("Function split_into_nhot() may only be applied to a single-column "
            "Frame of type string; got frame with 3 columns" == str(e.value))

    with pytest.raises(ValueError) as e:
        dt.split_into_nhot(f0[:, 0])
    assert ("Function split_into_nhot() may only be applied to a single-column "
            "Frame of type string; received a column of type r8" ==
            str(e.value))
