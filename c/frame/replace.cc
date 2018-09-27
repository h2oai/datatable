//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include <unordered_set>
#include "python/dict.h"
#include "python/list.h"
#include "utils/assert.h"

namespace py {


PKArgs Frame::Type::args_replace(
  2, 0, 0, false, false,
  {"to_replace", "replace_with"},
  "replace",
R"(replace(self, replace_what, replace_with)
--

Replace given value(s) `replace_what` with `replace_with` in the entire Frame.

For each replace value, this method operates only on columns of types
appropriate for that value. For example, if `replace_what` is a list
`[-1, math.inf, None, "??"]`, then the value `-1` will be replaced in integer
columns only, `math.inf` only in real columns, `None` in columns of all types,
and finally `"??"` only in string columns.

The replacement value must match the type of the value being replaced,
otherwise an exception will be thrown. That is, a bool must be replace with a
bool, an int with an int, a float with a float, and a string with a string.
The `None` value (representing NA) matches any column type, and therefore can
be used as either replacement target, or replace value for any column. In
particular, the following is valid: `DT.replace(None, [-1, -1.0, ""])`. This
will replace NA values in int columns with `-1`, in real columns with `-1.0`,
and in string columns with an empty string.

However, may cause a column to change
its stype, provided that ltype remains unmodified. For example, replacing
`0` with `-999` within an `int8` column will cause that column to be converted
into the `int16` stype.

Parameters
----------
replace_what: None, bool, int, float, list, or dict
    Value(s) to search for and replace.

replace_with: single value, or list
    The replacement value(s). If `replace_what` is a single value, then this
    must be a single value too. If `replace_what` is a list, then this could
    be either a single value, or a list of the same length. If `replace_what`
    is a dict, then this value should not be passed.

Returns
-------
Nothing, replacement is performed in-place.

Examples
--------
>>> df = dt.Frame([1, 2, 3] * 3)
>>> df.replace(1, -1)
>>> df.topython()
[[-1, 2, 3, -1, 2, 3, -1, 2, 3]]

>>> df.replace({-1: 100, 2: 200, "foo": None})
>>> df.topython()
[[100, 200, 3, 100, 200, 3, 100, 200, 3]]
)");


class ReplaceAgent {
  private:
    DataTable* dt;
    // vx, vy are simple lists of source/target values for replacement
    std::vector<py::obj> vx, vy;

    std::vector<int8_t> x_bool, y_bool;
    std::vector<int64_t> x_int, y_int;
    std::vector<double> x_real, y_real;
    std::vector<std::string> x_str, y_str;
    int64_t xmin_int, xmax_int;
    double xmin_real, xmax_real;

  public:
    ReplaceAgent(DataTable* _dt) : dt(_dt) {}
    void parse_x_y(const Arg& x, const Arg& y);
    void split_x_y_by_type();
    template <typename T> void process_int_column(size_t i);
    template <typename T> void process_real_column(size_t i);
    template <typename T> void process_str_column(size_t i);
    template <typename T>
    void replace_in_data(size_t n, T* x, T* y, size_t nrows, T* data);

  private:
    void split_x_y_bool();
    void split_x_y_int();
    void split_x_y_real();
    void split_x_y_str();
};




void Frame::replace(const PKArgs& args) {
  const Arg& x = args[0];  // replace what
  const Arg& y = args[1];  // replace with

  ReplaceAgent ra(dt);
  ra.parse_x_y(x, y);
  ra.split_x_y_by_type();

  size_t ncols = static_cast<size_t>(dt->ncols);
  for (size_t i = 0; i < ncols; ++i) {
    Column* col = dt->columns[i];
    switch (col->stype()) {
      case SType::INT8:  ra.process_int_column<int8_t>(i); break;
      case SType::INT16: ra.process_int_column<int16_t>(i); break;
      case SType::INT32: ra.process_int_column<int32_t>(i); break;
      case SType::INT64: ra.process_int_column<int64_t>(i); break;
      default: break;
    }
  }
}



//------------------------------------------------------------------------------
// Step 1: parse input arguments
//
// There are multiple different calling conventions for the `Frame.replace()`
// method; here we handle them, creating a unified representation in the form
// of two vectors `vx`, `vy` of values that need to be replaced and their
// replacements respectively.
//------------------------------------------------------------------------------

void ReplaceAgent::parse_x_y(const Arg& x, const Arg& y) {
  if (x.is_dict()) {
    if (y) {
      throw TypeError() << "When the first argument to Frame.replace() is a "
        "dict, there should be no second argument";
    }
    for (auto kv : rdict(x)) {
      vx.push_back(kv.first);
      vy.push_back(kv.second);
    }
    return;
  }
  else {
    if (x.is_list_or_tuple()) {
      olist xl = x.to_pylist();
      for (size_t i = 0; i < xl.size(); ++i) {
        vx.push_back(xl[i]);
      }
    } else {
      vx.push_back(x);
    }
    if (y.is_list_or_tuple()) {
      olist yl = y.to_pylist();
      if (vx.size() == 1 && vx[0].is_none()) {
        for (size_t i = 0; i < yl.size(); ++i) {
          vx.push_back(vx[0]);
        }
      }
      if (vx.size() != yl.size()) {
        throw ValueError() << "The `replace_what` and `replace_with` lists in "
          "Frame.replace() have different lengths: " << vx.size() << " and "
          << yl.size() << " respectively";
      }
      for (size_t i = 0; i < yl.size(); ++i) {
        vy.push_back(yl[i]);
      }
    } else {
      for (size_t i = 0; i < vx.size(); ++i) {
        vy.push_back(y);
      }
    }
  }
  xassert(vx.size() == vy.size());
}



//------------------------------------------------------------------------------
// Step 2: split lists vx, vy by types
//
// Here we analyze the input lists `vx`, `vy` and split them into 4 sublists
// according to their elements types. We also do further verification that the
// types of elements in vectors `vx`, `vy` match, and that there no duplicates.
//------------------------------------------------------------------------------

void ReplaceAgent::split_x_y_by_type() {
  bool done_bool = false,
       done_int = false,
       done_real = false,
       done_str = false;
  size_t ncols = static_cast<size_t>(dt->ncols);
  for (size_t i = 0; i < ncols; ++i) {
    SType s = dt->columns[i]->stype();
    switch (s) {
      case SType::BOOL: {
        if (done_bool) continue;
        split_x_y_bool();
        done_bool = true;
        break;
      }
      case SType::INT8:
      case SType::INT16:
      case SType::INT32:
      case SType::INT64: {
        if (done_int) continue;
        split_x_y_int();
        done_int = true;
        break;
      }
      case SType::FLOAT32:
      case SType::FLOAT64: {
        if (done_real) continue;
        split_x_y_real();
        done_real = true;
        break;
      }
      case SType::STR32:
      case SType::STR64: {
        if (done_str) continue;
        // split_x_y_str();
        done_str = true;
        break;
      }
      default: break;
    }
  }
}


void ReplaceAgent::split_x_y_bool() {
  size_t n = vx.size();
  for (size_t i = 0; i < n; ++i) {
    py::obj xelem = vx[i];
    py::obj yelem = vy[i];
    if (xelem.is_none()) {
      if (yelem.is_none()) continue;
      if (!yelem.is_bool()) continue;
      x_bool.push_back(GETNA<int8_t>());
      y_bool.push_back(yelem.to_bool());
    }
    else if (xelem.is_bool()) {
      if (!(yelem.is_none() || yelem.is_bool())) {
        throw TypeError() << "Cannot replace boolean value " << xelem
          << " with a value of type " << yelem.typeobj();
      }
      x_bool.push_back(xelem.to_bool());
      y_bool.push_back(yelem.to_bool());
    }
  }
}


void ReplaceAgent::split_x_y_int() {
  int64_t na_repl = GETNA<int64_t>();
  size_t n = vx.size();
  xmin_int = std::numeric_limits<int64_t>::max();
  xmax_int = std::numeric_limits<int64_t>::min();
  for (size_t i = 0; i < n; ++i) {
    py::obj xelem = vx[i];
    py::obj yelem = vy[i];
    if (xelem.is_none()) {
      if (yelem.is_none() || !yelem.is_int()) continue;
      na_repl = yelem.to_int64();
    }
    else if (xelem.is_int()) {
      if (!(yelem.is_none() || yelem.is_int())) {
        throw TypeError() << "Cannot replace integer value " << xelem
          << " with a value of type " << yelem.typeobj();
      }
      int64_t xval = xelem.to_int64();
      int64_t yval = yelem.to_int64();
      x_int.push_back(xval);
      y_int.push_back(yval);
      if (xval < xmin_int) xmin_int = xval;
      if (xval > xmax_int) xmax_int = xval;
    }
  }
  if (!ISNA(na_repl)) {
    x_int.push_back(GETNA<int64_t>());
    y_int.push_back(na_repl);
  }
  // Check that the values are unique
  std::unordered_set<int64_t> check;
  for (int64_t x : x_int) {
    if (check.count(x)) {
      throw ValueError() << "Replacement target `" << x << "` was specified "
        "more than once in Frame.replace()";
    }
    check.insert(x);
  }
}


void ReplaceAgent::split_x_y_real() {
  size_t n = vx.size();
  for (size_t i = 0; i < n; ++i) {
    py::obj xelem = vx[i];
    py::obj yelem = vy[i];
    if (xelem.is_none()) {
      if (yelem.is_none()) continue;
      if (!yelem.is_float()) continue;
      x_real.push_back(GETNA<double>());
      y_real.push_back(yelem.to_double());
    }
    else if (xelem.is_float()) {
      if (!(yelem.is_none() || yelem.is_float())) {
        throw TypeError() << "Cannot replace float value `" << xelem
          << "` with a value of type " << yelem.typeobj();
      }
      x_real.push_back(xelem.to_double());
      y_real.push_back(yelem.to_double());
    }
  }
}



//------------------------------------------------------------------------------
// Step 3: prepare data for replacement for each column in the Frame
//
// For each column, the list of values to replace is further trimmed according
// to the column's min/max value and presence of NAs. Additionally, a column
// may be upcast to a higher stype, if we detect that the replacement value is
// too large to fit into the current stype.
//------------------------------------------------------------------------------

template <typename T>
void ReplaceAgent::process_int_column(size_t colidx) {
  if (x_int.empty()) return;
  auto col = static_cast<IntColumn<T>*>(dt->columns[colidx]);
  int64_t col_min = col->min();
  int64_t col_max = col->max();
  bool col_has_nas = (col->countna() > 0);
  // xmax_int will be NA iff the replacement list has just one element: NA
  if (ISNA<int64_t>(xmax_int)) {
    if (!col_has_nas) return;
  } else {
    if (col_min > xmax_int || col_max < xmin_int) return;
  }
  // Prepare filtered x and y vectors
  std::vector<T> xfilt, yfilt;
  int64_t maxy = 0;
  for (size_t i = 0; i < x_int.size(); ++i) {
    int64_t x = x_int[i];
    if (ISNA(x)) {
      if (!col_has_nas) continue;
      xfilt.push_back(GETNA<T>());
    } else {
      if (x < col_min || x > col_max) continue;
      xfilt.push_back(static_cast<T>(x));
    }
    int64_t y = y_int[i];
    if (ISNA(y)) {
      yfilt.push_back(GETNA<T>());
    } else if (std::abs(y) <= std::numeric_limits<T>::max()) {
      yfilt.push_back(static_cast<T>(y));
    } else {
      maxy = std::abs(y);
    }
  }
  if (maxy) {
    SType new_stype = (maxy > std::numeric_limits<int32_t>::max())
                      ? SType::INT64 : SType::INT32;
    Column* newcol = col->cast(new_stype);
    dt->columns[colidx] = newcol;
    delete col;
    if (new_stype == SType::INT64) {
      process_int_column<int64_t>(colidx);
    } else {
      process_int_column<int32_t>(colidx);
    }
  } else {
    size_t n = xfilt.size();
    xassert(n == yfilt.size());
    size_t nrows = static_cast<size_t>(col->nrows);
    T* coldata = col->elements_w();
    replace_in_data<T>(n, xfilt.data(), yfilt.data(),
                       nrows, coldata);
  }
}




//------------------------------------------------------------------------------
// Step 4: perform actual data replacement
//------------------------------------------------------------------------------

template <typename T>
void ReplaceAgent::replace_in_data(size_t n, T* x, T* y, size_t nrows, T* data)
{
  xassert(n > 0);
  if (n == 1) {
    T x0 = x[0], y0 = y[0];
    #pragma omp parallel for
    for (size_t i = 0; i < nrows; ++i) {
      if (data[i] == x0) data[i] = y0;
    }
  }
  else if (n == 2) {
    T x0 = x[0], y0 = y[0];
    T x1 = x[1], y1 = y[1];
    #pragma omp parallel for
    for (size_t i = 0; i < nrows; ++i) {
      T v = data[i];
      if (v == x0) data[i] = y0;
      else if (v == x1) data[i] = y1;
    }
  }
  else {
    #pragma omp parallel for
    for (size_t i = 0; i < nrows; ++i) {
      T v = data[i];
      for (size_t j = 0; j < n; ++j) {
        if (v == x[j]) {
          data[i] = y[j];
          break;
        }
      }
    }
  }
}





}  // namespace py
