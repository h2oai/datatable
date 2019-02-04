#!/usr/bin/env python
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
import pytest
import random
from datatable.xls import (
    _parse_row, _combine_ranges, _process_merged_cells,
    _range2d_to_excel_coords, _excel_coords_to_range2d)



#-------------------------------------------------------------------------------
# Test internal helper functions
#-------------------------------------------------------------------------------

def test__parse_row():
    # Empty strings are: (a) type 0 or 6, (b) type 1 (text) with value "" or
    # whitespace-only.
    res = _parse_row([None, 1., 0., "", None, None, "  \t \n"],
                     [0, 2, 2, 1, 6, 5, 1])
    assert res == [(1, 3), (5, 6)]


def test__combine_ranges():
    cr = _combine_ranges
    res1 = cr([[(2, 6)]] * 5)
    assert res1 == [[0, 5, 2, 6]]
    res2 = cr([[(1, 2)], [(0, 1)]])
    assert res2 == [[1, 2, 0, 1], [0, 1, 1, 2]]
    res3 = cr([[], [], [(2, 4)], [(2, 4)], [(0, 3)]])
    assert res3 == [[2, 5, 0, 4]]
    res4 = cr([[(2, 5)], [(1, 3), (4, 6)], []])
    assert res4 == [[0, 2, 1, 6]]
    res5 = cr([[(1, 4), (5, 8)], [(0, 2), (3, 6)]])
    assert res5 == [[0, 2, 0, 8]]
    res6 = cr([[], [(1, 6)], [(1, 2), (5, 6)],
               [(1, 2), (3, 4), (5, 6)], [(1, 2), (5, 6)], [(1, 6)]])
    assert res6 == [[1, 6, 1, 6]]
    assert (cr([[(1, 2), (3, 4)], [(0, 7)], [(2, 3)], [(3, 4)]]) ==
            [[0, 4, 0, 7]])
    assert (cr([[(0, 1)], [(0, 1), (2, 3)], [(0, 3)]]) ==
            [[0, 3, 0, 3]])
    assert (cr([[(2, 3)], [(0, 1), (2, 3)], [(0, 3), (6, 7)]]) ==
            [[0, 3, 0, 3], [2, 3, 6, 7]])


def test__process_merged_cells():
    ranges = [[0, 12, 0, 10], [4, 7, 13, 17]]
    merged = [[2, 3, 1, 2], [11, 13, 4, 5], [5, 6, 8, 13]]
    _process_merged_cells(ranges, merged)
    assert ranges == [[0, 13, 0, 17]]


def test__range2d_to_excel():
    conv = _range2d_to_excel_coords
    assert conv([1, 6, 1, 6]) == "B2:F6"
    for i in range(1, 100):
        A = ord('A')
        j = i - 1
        if j < 26:
            col = chr(A + j)
        else:
            col = chr(A + (j // 26) - 1) + chr(A + j % 26)
        assert conv([0, i, 0, i]) == "A1:%s%d" % (col, i)


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test__excel_coords_to_range2d(seed):
    random.seed(seed)
    for _ in range(100):
        row0 = int(random.expovariate(0.001))
        row1 = int(random.expovariate(0.001)) + row0 + 1
        col0 = int(random.expovariate(0.001))
        col1 = int(random.expovariate(0.001)) + col0 + 1
        range2d = (row0, row1, col0, col1)
        excel_coords = _range2d_to_excel_coords(range2d)
        assert _excel_coords_to_range2d(excel_coords) == range2d
