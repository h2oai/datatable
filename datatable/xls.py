#!/usr/bin/env python3
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
import re
from datatable.utils.typechecks import TValueError



def read_xls_workbook(filename, subpath):
    try:
        import xlrd
    except ImportError:
        raise TValueError("Module `xlrd` is required in order to read "
                          "Excel file '%s'" % filename)

    if subpath:
        wb = xlrd.open_workbook(filename, on_demand=True, ragged_rows=True)
        range2d = None
        if subpath in wb.sheet_names():
            sheetname = subpath
        else:
            if "/" in subpath:
                sheetname, xlsrange = subpath.rsplit('/', 1)
                range2d = _excel_coords_to_range2d(xlsrange)
            if not(sheetname in wb.sheet_names() and range2d is not None):
                raise TValueError("Sheet `%s` is not found in the XLS file"
                                  % subpath)
        ws = wb.sheet_by_name(sheetname)
        result = read_xls_worksheet(ws, range2d)
    else:
        wb = xlrd.open_workbook(filename, ragged_rows=True)
        result = {}
        for ws in wb.sheets():
            out = read_xls_worksheet(ws)
            if out is None:
                continue
            for i, frame in out.items():
                result["%s/%s" % (ws.name, i)] = frame

    if len(result) == 0:
        return None
    elif len(result) == 1:
        for v in result.values():
            return v
    else:
        return result



def read_xls_worksheet(ws, subrange=None):
    # Get the worksheet's internal data arrays directly, for efficienct
    values = ws._cell_values
    types = ws._cell_types
    assert len(values) == len(types)
    # If the worksheet is empty, skip it
    if not values:
        return None

    if subrange is None:
        ranges2d = _combine_ranges([_parse_row(values[i], types[i])
                                    for i in range(len(values))])
        ranges2d.sort(key=lambda x: -(x[1] - x[0]) * (x[3] - x[2]))
    else:
        ranges2d = [subrange]

    results = {}
    for range2d in ranges2d:
        row0, row1, col0, col1 = range2d
        ncols = col1 - col0
        if row0 < len(values):
            colnames = [str(n) for n in values[row0][col0:col1]]
            if len(colnames) < ncols:
                colnames += [None] * (ncols - len(colnames))
        else:
            colnames = [None] * ncols
        rowdata = []
        if row1 > len(values):
            values += [[]] * (row1 - len(values))
        for irow in range(row0 + 1, row1):
            vv = values[irow]
            row = tuple(vv[col0:col1])
            if len(row) < ncols:
                row += (None,) * (ncols - len(row))
            rowdata.append(row)
        frame = dt.Frame(rowdata, names=colnames)
        results[_range2d_to_excel_coords(range2d)] = frame
    return results



def _parse_row(rowvalues, rowtypes):
    """
    Scan a single row from an Excel file, and return the list of ranges
    corresponding to each consecutive span of non-empty cells in this row.
    If all cells are empty, return an empty list. Each "range" in the list
    is a tuple of the form `(startcol, endcol)`.

    For example, if the row is the following:

        [ ][ 1.0 ][ 23 ][ "foo" ][ ][ "hello" ][ ]

    then the returned list of ranges will be:

        [(1, 4), (5, 6)]

    This algorithm considers a cell to be empty if its type is 0 (XL_EMPTY),
    or 6 (XL_BLANK), or if it's a text cell containing empty string, or a
    whitespace-only string. Numeric `0` is not considered empty.
    """
    n = len(rowvalues)
    assert n == len(rowtypes)
    if not n:
        return []
    range_start = None
    ranges = []
    for i in range(n):
        ctype = rowtypes[i]
        cval = rowvalues[i]
        # Check whether the cell is empty or not. If it is empty, and there is
        # an active range being tracked - terminate it. On the other hand, if
        # the cell is not empty and there isn't an active range, then start it.
        if ctype == 0 or ctype == 6 or (ctype == 1 and
                                        (cval == "" or cval.isspace())):
            if range_start is not None:
                ranges.append((range_start, i))
                range_start = None
        else:
            if range_start is None:
                range_start = i
    if range_start is not None:
        ranges.append((range_start, n))
    return ranges


def _combine_ranges(ranges):
    """
    This function takes a list of row-ranges (as returned by `_parse_row`)
    ordered by rows, and produces a list of distinct rectangular ranges
    within this grid.

    Within this function we define a 2d-range as a rectangular set of cells
    such that:
      - there are no empty rows / columns within this rectangle;
      - the rectangle is surrounded by empty rows / columns on all sides;
      - no subset of this rectangle comprises a valid 2d-range;
      - separate 2d-ranges are allowed to touch at a corner.
    """
    ranges2d = []
    for irow, rowranges in enumerate(ranges):
        ja = 0
        jc = 0
        while jc < len(rowranges):
            ccol0, ccol1 = rowranges[jc]
            if ja < len(ranges2d):
                arow0, arow1, acol0, acol1 = ranges2d[ja]
                if arow1 < irow:
                    ja += 1
                    continue
                assert arow1 == irow or arow1 == irow + 1
            else:
                acol0 = acol1 = 1000000000

            if ccol0 == acol0 and ccol1 == acol1:
                ranges2d[ja][1] = irow + 1
                ja += 1
                jc += 1
            elif ccol1 <= acol0:
                ranges2d.insert(ja, [irow, irow + 1, ccol0, ccol1])
                ja += 1
                jc += 1
            elif ccol0 >= acol1:
                ja += 1
            else:
                assert ja < len(ranges2d)
                ranges2d[ja][1] = irow + 1
                if ccol0 < acol0:
                    ranges2d[ja][2] = ccol0
                if ccol1 > acol1:
                    ranges2d[ja][3] = acol1 = ccol1
                    jn = 0
                    while jn < len(ranges2d):
                        if jn == ja:
                            jn += 1
                            continue
                        nrow0, nrow1, ncol0, ncol1 = ranges2d[jn]
                        if ncol0 <= acol1 and nrow1 >= arow0 and \
                                not(ncol0 == acol1 and nrow1 == arow0):
                            ranges2d[ja][0] = arow0 = min(arow0, nrow0)
                            ranges2d[ja][3] = acol1 = max(acol1, ncol1)
                            del ranges2d[jn]
                            if jn < ja:
                                ja -= 1
                        else:
                            jn += 1
                jc += 1
    return ranges2d


def _range2d_to_excel_coords(range2d):
    def colname(i):
        r = ""
        while i >= 0:
            r = chr(ord('A') + i % 26) + r
            i = (i // 26) - 1
        return r

    row0, row1, col0, col1 = range2d
    return "%s%d:%s%d" % (colname(col0), row0 + 1,
                          colname(col1 - 1), row1)


def _excel_coords_to_range2d(ec):
    def colindex(n):
        i = 0
        for c in n:
            i = i * 26 + (ord(c) - ord('A')) + 1
        return i - 1

    mm = re.match("([A-Z]+)(\d+):([A-Z]+)(\d+)", ec)
    if not mm:
        return None
    row0 = int(mm.group(2)) - 1
    row1 = int(mm.group(4))
    col0 = colindex(mm.group(1))
    col1 = colindex(mm.group(3)) + 1
    if row0 > row1:
        row0, row1 = row1, row0
    if col0 > col1:
        col0, col1 = col1, col0
    return (row0, row1, col0, col1)
