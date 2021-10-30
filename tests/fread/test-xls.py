#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018-2021 H2O.ai
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
import os
import pytest
import random
import datatable as dt
from tests import find_file, assert_equals, frame_to_xlsx
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


def test_xlsx_simple(tempfile_xlsx):
    dt0 = dt.Frame({"A": [-1, 7, 10000, 12],
                    "B": [True, None, False, None],
                    "C": ["alpha", "beta", None, "delta"]})
    frame_to_xlsx(dt0, tempfile_xlsx)
    dt1 = dt.Frame(tempfile_xlsx)
    assert dt1.source == tempfile_xlsx
    assert_equals(dt0, dt1)


def test_xlsx_subpath(tempfile_xlsx):
    dt0 = dt.Frame({"A": [-1, 7, 10000, 12],
                    "B": [True, None, False, None],
                    "C": ["alpha", "beta", None, "delta"]})
    frame_to_xlsx(dt0, tempfile_xlsx)
    sub_worksheet = tempfile_xlsx + "/Sheet/A1:B3"
    dt1 = dt.Frame(sub_worksheet)
    assert dt1.source == sub_worksheet
    assert_equals(dt0[:2, :2], dt1)


def test_xlsx_all_types(tempfile_xlsx):
    from datetime import datetime
    dt0 = dt.Frame([[True, False, None, True, True],
                   [None, 1, -9, 12, 3],
                   [4, 1346, 999, None, None],
                   [591, 0, None, -395734, 19384709],
                   [None, 777, 1093487019384, -384, None],
                   [2.987, 3.45e-24, -0.189134e+12, 45982.1, None],
                   [39408.301, 9.459027045e-125, 4.4508e+222, None, 3.14159],
                   [datetime(2021, 5, 6), datetime(2019, 9, 6),
                    datetime(2019, 5, 6), None, datetime(2000, 12, 6)],
                   ["Life", "Liberty", "and", "Pursuit of Happiness", None],
                   ["кохайтеся", "чорнобриві", ",", "та", "не з москалями"]
                   ],
                  stypes=[dt.bool8, dt.int32, dt.int32, dt.int32, dt.int64,
                          dt.float64, dt.float64, dt.Type.time64, dt.str32,
                          dt.str32],
                  names=["b8", "i32", "i32", "i32", "i32", "f64", "f64",
                         "time64", "s32", "s32"])
    frame_to_xlsx(dt0, tempfile_xlsx)
    dt1 = dt.Frame(tempfile_xlsx)
    assert dt1.source == tempfile_xlsx
    assert_equals(dt0, dt1)


def test_10k_diabetes_xlsx():
    filename = find_file("h2o-3", "fread", "10k_diabetes.xlsx")
    DT = dt.fread(filename)
    assert DT.source == filename
    assert DT.shape == (10000, 51)
    assert DT.names[:4] == ("race", "gender", "age", "weight")
    assert DT["readmitted"].stype == dt.bool8
    assert DT[:, "num_lab_procedures":"number_inpatient"].stype == dt.int32
    assert dt.unique(DT["gender"]).nrows == 2


def test_large_ids_xlsx():
    filename = find_file("h2o-3", "fread", "large_ids.xlsx")
    DT = dt.fread(filename)
    assert DT.source == filename
    assert DT.shape == (53493, 3)
    assert DT.stypes == (dt.int32, dt.int32, dt.int32)
    assert DT.names == ("C1", "member_id", "loan_amnt")
    assert DT.sum().to_tuples() == [(14436256829, 810493167234, 721084925)]


def test_set_xls_new_xlsx():
    from datetime import datetime
    filename = find_file("h2o-3", "fread", "test_set_xls_new.xlsx")
    DT = dt.fread(filename)
    assert DT.source == filename
    assert DT.shape == (7, 1)
    assert DT.names == ("Data",)
    assert DT.to_list()[0] == [datetime(2019, 5, 6),
                               datetime(2019, 5, 7),
                               datetime(2019, 5, 8),
                               datetime(2019, 5, 9),
                               datetime(2019, 5, 10),
                               datetime(2019, 5, 11),
                               datetime(2019, 5, 12)]


def test_diabetes_tiny_two_sheets_xlsx():
    filename = find_file("h2o-3", "fread", "diabetes_tiny_two_sheets.xlsx")
    DTs_keys = [filename + "/Sheet1/A1:AY17", filename + "/Sheet2/A1:AY17"]
    DT1, DT2 = list(dt.iread(filename))
    assert sorted([DT1.source, DT2.source]) == DTs_keys
    assert DT1.shape == DT2.shape == (16, 51)
    assert DT1.stypes == DT2.stypes
    assert DT1.to_list() == DT2.to_list()


def test_winemag_data_rate_wine_xlsx():
    filename = find_file("h2o-3", "fread", "winemag-data_rate_wine.xlsx")
    DT = dt.fread(filename)
    dt.internal.frame_integrity_check(DT)
    assert DT.source == filename
    assert DT.shape == (2007, 9)
    assert DT.names == ('Sno', 'country', 'description', 'designation', 'points',
                        'price', 'province', 'variety', 'winery')
    assert DT.stypes == (dt.int32, dt.str32, dt.str32, dt.str32, dt.int32,
                         dt.int32, dt.str32, dt.str32, dt.str32)
    assert DT['country'].nunique1() == 15
    assert abs(DT['price'].mean1() - 24.7937) < 0.001


def test_excel_testbook_xlsx_1():
    filename = find_file("h2o-3", "fread", "excelTestbook.xlsx")
    DT1 = dt.fread(filename + "/Sheet1")
    assert DT1.source == os.path.abspath(filename) + "/Sheet1"
    assert_equals(DT1, dt.Frame([("Apples", 50),
                                 ("Oranges", 20),
                                 ("Bananas", 60),
                                 ("Lemons", 40),
                                 (None, 170)], names=["Fruit", "Amount"]))


def test_excel_testbook_xlsx_2():
    filename = find_file("h2o-3", "fread", "excelTestbook.xlsx")
    DT2 = dt.fread(filename + "/Sheet2")
    assert DT2.source == os.path.abspath(filename) + "/Sheet2"
    assert_equals(DT2, dt.Frame(day=["today", "tomorrow", "yes\nter\nday",
                                     "everyday"]))


def test_excel_testbook_xlsx_3():
    from datetime import datetime
    filename = find_file("h2o-3", "fread", "excelTestbook.xlsx")
    DT3 = dt.fread(filename + "/big sheet")
    assert DT3.source == os.path.abspath(filename) + "/big sheet"
    assert DT3.shape == (20, 2)
    assert DT3.names == ("date1", "time1")
    assert DT3['date1'].to_list()[0] == [datetime(2020, 1, i)
                                         for i in range(1, 21)]
    assert DT3['time1'].to_list()[0] == [datetime(2020, 1, 1, i, 24, 30)
                                         for i in range(1, 10)] + \
                                        [None] * 11


def test_excel_testbook_xlsx_4():
    filename = find_file("h2o-3", "fread", "excelTestbook.xlsx")
    DT3 = dt.fread(filename + "/next sheet")
    assert DT3.source == os.path.abspath(filename) + "/next sheet"
    assert DT3.stypes == (dt.bool8, dt.int32, dt.bool8, dt.int32, dt.float64)
    assert DT3.names == ("colA", "colB", "colC", "colD", "colE")
    assert DT3.to_list() == [
        [True, False, True, False],
        [5, 7, 12, 0],
        [False, True, True, False],
        [None, None, 6, 8],
        [3, 7, 2.5, 11]]


def test_excel_testbook_xlsx_5():
    filename = find_file("h2o-3", "fread", "excelTestbook.xlsx")
    DT3 = dt.fread(filename + "/ragged/B2:E8")
    assert DT3.source == os.path.abspath(filename) + "/ragged/B2:E8"
    assert DT3.names == ("a", "C0", "b", "c")
    assert DT3.stype == dt.int32
    assert DT3.to_list() == [[None, 7, None, None, 0, None],
                             [1, 5, None, 3, None, None],
                             [None, None, None, None, None, 4],
                             [None, None, 12, None, 9, None]]
