//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include <type_traits>
#include <unordered_set>
#include "documentation.h"
#include "frame/py_frame.h"
#include "parallel/api.h"           // dt::parallel_for_static
#include "parallel/string_utils.h"  // dt::map_str2str
#include "python/dict.h"
#include "python/list.h"
#include "utils/assert.h"
#include "utils/macros.h"
#include "stype.h"
namespace py {


//------------------------------------------------------------------------------
// ReplaceAgent
//------------------------------------------------------------------------------

class ReplaceAgent {
  private:
    DataTable* dt;
    // vx, vy are simple lists of source/target values for replacement
    std::vector<py::robj> vx, vy;

    std::vector<int8_t> x_bool, y_bool;
    std::vector<int64_t> x_int, y_int;
    std::vector<double> x_real, y_real;
    std::vector<dt::CString> x_str, y_str;
    int64_t xmin_int, xmax_int;
    double xmin_real, xmax_real;

    bool columns_cast;
    size_t : 56;

  public:
    explicit ReplaceAgent(DataTable* _dt)
      : dt(_dt), xmin_int(0), xmax_int(0), xmin_real(0), xmax_real(0),
        columns_cast(false) {}
    void parse_x_y(const Arg& x, const Arg& y);
    void split_x_y_by_type();
    void process_bool_column(size_t i);
    template <typename T> void process_int_column(size_t i);
    template <typename T> void process_real_column(size_t i);
    template <typename T> void replace_fw(T* x, T* y, size_t nrows, T* data, size_t n);
    template <typename T> void replace_fw1(T* x, T* y, size_t nrows, T* data);
    template <typename T> void replace_fw2(T* x, T* y, size_t nrows, T* data);
    template <typename T> void replace_fwN(T* x, T* y, size_t nrows, T* data, size_t n);
    void process_str_column(size_t i);
    Column replace_str(size_t n, dt::CString* x, dt::CString* y, const Column&);
    Column replace_str1(dt::CString* x, dt::CString* y, const Column&);
    Column replace_str2(dt::CString* x, dt::CString* y, const Column&);
    Column replace_strN(dt::CString* x, dt::CString* y, const Column&, size_t n);
    bool types_changed() const { return columns_cast; }

  private:
    void split_x_y_bool();
    void split_x_y_int();
    void split_x_y_real();
    void split_x_y_str();
    template <typename T> void check_uniqueness(std::vector<T>&);
};




//------------------------------------------------------------------------------
// Frame::replace()
//------------------------------------------------------------------------------

static PKArgs args_replace(
  2, 0, 0, false, false,
  {"to_replace", "replace_with"}, "replace", dt::doc_Frame_replace);


void Frame::replace(const PKArgs& args) {
  const Arg& x = args[0];  // replace what
  const Arg& y = args[1];  // replace with
  if (!x) {
    throw TypeError() << "Missing the required argument `replace_what` in "
        "method Frame.replace()";
  }
  if (dt->nkeys()) {
    throw ValueError() << "Cannot replace values in a keyed frame";
  }

  ReplaceAgent ra(dt);
  ra.parse_x_y(x, y);
  ra.split_x_y_by_type();

  for (size_t i = 0; i < dt->ncols(); ++i) {
    // If a column is a view, then: for a fixed-width column it gets
    // materialized when we request `col.get_data_editable()`;
    // on the other hand, a string column remains a view, however the
    // iterator `dt::map_str2str`  takes the rowindex into account
    // when iterating.
    //
    const Column& col = dt->get_column(i);
    switch (col.stype()) {
      case dt::SType::BOOL:    ra.process_bool_column(i); break;
      case dt::SType::INT8:    ra.process_int_column<int8_t>(i); break;
      case dt::SType::INT16:   ra.process_int_column<int16_t>(i); break;
      case dt::SType::INT32:   ra.process_int_column<int32_t>(i); break;
      case dt::SType::INT64:   ra.process_int_column<int64_t>(i); break;
      case dt::SType::FLOAT32: ra.process_real_column<float>(i); break;
      case dt::SType::FLOAT64: ra.process_real_column<double>(i); break;
      case dt::SType::STR32:
      case dt::SType::STR64:   ra.process_str_column(i); break;
      default: break;
    }
  }
  if (ra.types_changed()) _clear_types();
  source_ = nullptr;
}


void Frame::_init_replace(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::replace, args_replace));
}



//------------------------------------------------------------------------------
// Step 1: parse input arguments
//
// There are multiple different calling signatures for the `Frame.replace()`
// method. Here we handle them, creating a unified representation in the form
// of two vectors `vx`, `vy` of values that need to be replaced and their
// replacements respectively.
//------------------------------------------------------------------------------

void ReplaceAgent::parse_x_y(const Arg& x, const Arg& y) {
  if (x.is_dict()) {
    if (y) {
      throw TypeError() << "When the first argument to Frame.replace() is a "
        "dictionary, there should be no other arguments";
    }
    for (const auto kv : x.to_rdict()) {
      vx.push_back(kv.first);
      vy.push_back(kv.second);
    }
    return;
  }
  else {
    if (!y) {
      throw TypeError() << "Missing the required argument `replace_with` in "
          "method Frame.replace()";
    }
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
        for (size_t i = 1; i < yl.size(); ++i) {
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
// types of elements in vectors `vx`, `vy` match, and that there are no
// duplicates.
//------------------------------------------------------------------------------

void ReplaceAgent::split_x_y_by_type() {
  bool done_int = false,
       done_real = false,
       done_bool = false,
       done_str = false;
  for (size_t i = 0; i < dt->ncols(); ++i) {
    dt::SType s = dt->get_column(i).stype();
    switch (s) {
      case dt::SType::BOOL: {
        if (done_bool) continue;
        split_x_y_bool();
        done_bool = true;
        break;
      }
      case dt::SType::INT8:
      case dt::SType::INT16:
      case dt::SType::INT32:
      case dt::SType::INT64: {
        if (done_int) continue;
        split_x_y_int();
        done_int = true;
        break;
      }
      case dt::SType::FLOAT32:
      case dt::SType::FLOAT64: {
        if (done_real) continue;
        split_x_y_real();
        done_real = true;
        break;
      }
      case dt::SType::STR32:
      case dt::SType::STR64: {
        if (done_str) continue;
        split_x_y_str();
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
    py::robj xelem = vx[i];
    py::robj yelem = vy[i];
    if (xelem.is_none()) {
      if (yelem.is_none()) continue;
      if (!yelem.is_bool() && !yelem.is_numpy_bool()) continue;
      x_bool.push_back(dt::GETNA<int8_t>());
      y_bool.push_back(yelem.to_bool_force());
    }
    else if (xelem.is_bool() || xelem.is_numpy_bool()) {
      if (!(yelem.is_none() || yelem.is_bool() || yelem.is_numpy_bool())) {
        throw TypeError() << "Cannot replace boolean value `" << xelem
          << "` with a value of type " << yelem.typeobj();
      }
      x_bool.push_back(xelem.to_bool_force());
      y_bool.push_back(yelem.to_bool_force());
    }
  }
  check_uniqueness<int8_t>(x_bool);
}


void ReplaceAgent::split_x_y_int() {
  int64_t na_repl = dt::GETNA<int64_t>();
  size_t n = vx.size();
  xmin_int = std::numeric_limits<int64_t>::max();
  xmax_int = -xmin_int;
  for (size_t i = 0; i < n; ++i) {
    py::robj xelem = vx[i];
    py::robj yelem = vy[i];
    if (xelem.is_none()) {
      if (yelem.is_none()) continue;
      if (!yelem.is_int() && !yelem.is_numpy_int()) continue;
      na_repl = yelem.to_int64();
    }
    else if (xelem.is_int() || xelem.is_numpy_int()) {
      if (!(yelem.is_none() || yelem.is_int() || yelem.is_numpy_int())) {
        throw TypeError() << "Cannot replace integer value `" << xelem
          << "` with a value of type " << yelem.typeobj();
      }
      int64_t xval = xelem.to_int64();
      int64_t yval = yelem.to_int64();
      x_int.push_back(xval);
      y_int.push_back(yval);
      if (xval < xmin_int) xmin_int = xval;
      if (xval > xmax_int) xmax_int = xval;
    }
  }
  if (!dt::ISNA(na_repl)) {
    x_int.push_back(dt::GETNA<int64_t>());
    y_int.push_back(na_repl);
  }
  check_uniqueness<int64_t>(x_int);
}


void ReplaceAgent::split_x_y_real() {
  double na_repl = dt::GETNA<double>();
  size_t n = vx.size();
  xmin_real = std::numeric_limits<double>::max();
  xmax_real = -xmin_real;
  for (size_t i = 0; i < n; ++i) {
    py::robj xelem = vx[i];
    py::robj yelem = vy[i];
    if (xelem.is_none()) {
      if (yelem.is_none()) continue;
      if (!yelem.is_float() && !yelem.is_numpy_float()) continue;
      na_repl = yelem.to_double();
    }
    else if (xelem.is_float() || xelem.is_numpy_float()) {
      if (!(yelem.is_none() || yelem.is_float() || yelem.is_numpy_float())) {
        throw TypeError() << "Cannot replace float value `" << xelem
          << "` with a value of type " << yelem.typeobj();
      }
      double xval = xelem.to_double();
      double yval = yelem.to_double();
      if (dt::ISNA(xval)) {
        na_repl = yval;
      } else {
        x_real.push_back(xval);
        y_real.push_back(yval);
        if (xval < xmin_real) xmin_real = xval;
        if (xval > xmax_real) xmax_real = xval;
      }
    }
  }
  if (!dt::ISNA(na_repl)) {
    x_real.push_back(dt::GETNA<double>());
    y_real.push_back(na_repl);
  }
  check_uniqueness<double>(x_real);
}


void ReplaceAgent::split_x_y_str() {
  size_t n = vx.size();
  dt::CString na_repl;
  for (size_t i = 0; i < n; ++i) {
    py::robj xelem = vx[i];
    py::robj yelem = vy[i];
    if (xelem.is_none()) {
      if (yelem.is_none() || !yelem.is_string()) continue;
      na_repl = yelem.to_cstring();
    }
    else if (xelem.is_string()) {
      if (!(yelem.is_none() || yelem.is_string())) {
        throw TypeError() << "Cannot replace string value `" << xelem
          << "` with a value of type " << yelem.typeobj();
      }
      x_str.push_back(xelem.to_cstring());
      y_str.push_back(yelem.to_cstring());
    }
  }
  if (!na_repl.isna()) {
    x_str.push_back(dt::CString());
    y_str.push_back(std::move(na_repl));
  }
}


template <typename T>
void ReplaceAgent::check_uniqueness(std::vector<T>& data) {
  std::unordered_set<T> check;
  for (const T& x : data) {
    if (check.count(x)) {
      throw ValueError() << "Replacement target `" << x << "` was specified "
        "more than once in Frame.replace()";
    }
    check.insert(x);
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

void ReplaceAgent::process_bool_column(size_t colidx) {
  if (x_bool.empty()) return;
  Column& col = dt->get_column(colidx);
  col.materialize();
  int8_t* coldata = static_cast<int8_t*>(col.get_data_editable());
  size_t n = x_bool.size();
  xassert(n == y_bool.size());
  if (n == 0) return;
  replace_fw<int8_t>(x_bool.data(), y_bool.data(), col.nrows(), coldata, n);
}


template <typename T>
void ReplaceAgent::process_int_column(size_t colidx) {
  if (x_int.empty()) return;
  Column& col = dt->get_column(colidx);
  col.materialize();

  int64_t col_min = col.stats()->min_int();
  int64_t col_max = col.stats()->max_int();

  bool col_has_nas = (col.na_count() > 0);
  if (xmin_int > xmax_int) {  // iff replace_what = [NA]
    if (!col_has_nas) return;
  } else {
    if (col_min > xmax_int || col_max < xmin_int) return;
  }
  // Prepare filtered x and y vectors
  std::vector<T> xfilt, yfilt;
  int64_t maxy = 0;
  for (size_t i = 0; i < x_int.size(); ++i) {
    int64_t x = x_int[i];
    if (dt::ISNA(x)) {
      if (!col_has_nas) continue;
      xfilt.push_back(dt::GETNA<T>());
    } else {
      if (x < col_min || x > col_max) continue;
      xfilt.push_back(static_cast<T>(x));
    }
    int64_t y = y_int[i];
    if (dt::ISNA(y)) {
      yfilt.push_back(dt::GETNA<T>());
    } else if (std::abs(y) <= std::numeric_limits<T>::max()) {
      yfilt.push_back(static_cast<T>(y));
    } else {
      maxy = std::abs(y);
    }
  }
  if (maxy) {
    dt::SType new_stype = (maxy > std::numeric_limits<int32_t>::max())
                      ? dt::SType::INT64 : dt::SType::INT32;
    dt->set_column(colidx, col.cast(new_stype));
    columns_cast = true;
    if (new_stype == dt::SType::INT64) {
      process_int_column<int64_t>(colidx);
    } else {
      process_int_column<int32_t>(colidx);
    }
  } else {
    size_t n = xfilt.size();
    xassert(n == yfilt.size());
    if (n == 0) return;
    T* coldata = static_cast<T*>(col.get_data_editable());
    replace_fw<T>(xfilt.data(), yfilt.data(), col.nrows(), coldata, n);
    col.reset_stats();
  }
}


template <typename T>
void ReplaceAgent::process_real_column(size_t colidx) {
  constexpr double MAX_FLOAT = double(std::numeric_limits<float>::max());
  if (x_real.empty()) return;
  Column& col = dt->get_column(colidx);
  col.materialize();
  double col_min = col.stats()->min_double();
  double col_max = col.stats()->max_double();

  bool col_has_nas = (col.na_count() > 0);
  if (xmin_real > xmax_real) {  // only when replace_what is [NA]
    if (!col_has_nas) return;
  } else {
    if (col_min > xmax_real || col_max < xmin_real) return;
  }
  // Prepare filtered x and y vectors
  std::vector<T> xfilt, yfilt;
  double maxy = 0;
  for (size_t i = 0; i < x_real.size(); ++i) {
    double x = x_real[i];
    if (dt::ISNA(x)) {
      if (!col_has_nas) continue;
      xassert(i == x_real.size() - 1);
      xfilt.push_back(dt::GETNA<T>());
    } else {
      if (x < col_min || x > col_max) continue;
      xfilt.push_back(static_cast<T>(x));
    }
    double y = y_real[i];
    if (dt::ISNA(y)) {
      yfilt.push_back(dt::GETNA<T>());
    } else if (std::is_same<T, double>::value || std::abs(y) <= MAX_FLOAT) {
      yfilt.push_back(static_cast<T>(y));
    } else {
      maxy = std::abs(y);
    }
  }
  if (std::is_same<T, float>::value && maxy > 0) {
    dt->set_column(colidx, col.cast(dt::SType::FLOAT64));
    columns_cast = true;
    process_real_column<double>(colidx);
  } else {
    size_t n = xfilt.size();
    xassert(n == yfilt.size());
    if (n == 0) return;
    T* coldata = static_cast<T*>(col.get_data_editable());
    replace_fw<T>(xfilt.data(), yfilt.data(), col.nrows(), coldata, n);
    col.reset_stats();
  }
}



void ReplaceAgent::process_str_column(size_t colidx) {
  if (x_str.empty()) return;
  const Column& col = dt->get_column(colidx);
  if (x_str.size() == 1 && x_str[0].isna()) {
    if (col.na_count() == 0) return;
  }
  Column newcol = replace_str(x_str.size(), x_str.data(), y_str.data(), col);
  columns_cast |= (newcol.stype() != col.stype());
  dt->set_column(colidx, std::move(newcol));
}




//------------------------------------------------------------------------------
// Step 4: perform actual data replacement
//------------------------------------------------------------------------------
// Ignore warnings in auto-generated lambda code
DISABLE_CLANG_WARNING("-Wpadded")


template <typename T>
void ReplaceAgent::replace_fw(T* x, T* y, size_t nrows, T* data, size_t n)
{
  if (n == 1) {
    replace_fw1<T>(x, y, nrows, data);
  }
  else if (n == 2) {
    replace_fw2<T>(x, y, nrows, data);
  }
  else {
    xassert(n > 0);
    replace_fwN<T>(x, y, nrows, data, n);
  }
}


template <typename T>
void ReplaceAgent::replace_fw1(T* x, T* y, size_t nrows, T* data) {
  T x0 = x[0], y0 = y[0];
  if (std::is_floating_point<T>::value && dt::ISNA<T>(x0)) {
    dt::parallel_for_static(nrows,
      [=](size_t i) {
        if (dt::ISNA<T>(data[i])) data[i] = y0;
      });
  } else {
    dt::parallel_for_static(nrows,
      [=](size_t i) {
        if (data[i] == x0) data[i] = y0;
      });
  }
}


template <typename T>
void ReplaceAgent::replace_fw2(T* x, T* y, size_t nrows, T* data) {
  T x0 = x[0], y0 = y[0];
  T x1 = x[1], y1 = y[1];
  xassert(!dt::ISNA<T>(x0));
  if (std::is_floating_point<T>::value && dt::ISNA<T>(x1)) {
    dt::parallel_for_static(nrows,
      [=](size_t i) {
        T v = data[i];
        if (v == x0) data[i] = y0;
        else if (dt::ISNA<T>(v)) data[i] = y1;
      });
  } else {
    dt::parallel_for_static(nrows,
      [=](size_t i) {
        T v = data[i];
        if (v == x0) data[i] = y0;
        else if (v == x1) data[i] = y1;
      });
  }
}


template <typename T>
void ReplaceAgent::replace_fwN(T* x, T* y, size_t nrows, T* data, size_t n) {
  if (std::is_floating_point<T>::value && dt::ISNA<T>(x[n-1])) {
    n--;
    dt::parallel_for_static(nrows,
      [=](size_t i) {
        T v = data[i];
        if (dt::ISNA<T>(v)) {
          data[i] = y[n];
          return;
        }
        for (size_t j = 0; j < n; ++j) {
          if (v == x[j]) {
            data[i] = y[j];
            break;
          }
        }
      });
  } else {
    dt::parallel_for_static(nrows,
      [=](size_t i) {
        T v = data[i];
        for (size_t j = 0; j < n; ++j) {
          if (v == x[j]) {
            data[i] = y[j];
            break;
          }
        }
      });
  }
}


Column ReplaceAgent::replace_str(size_t n, dt::CString* x, dt::CString* y,
                                  const Column& col)
{
  if (n == 1) {
    return replace_str1(x, y, col);
  } else {
    return replace_strN(x, y, col, n);
  }
}

Column ReplaceAgent::replace_str1(
    dt::CString* x, dt::CString* y, const Column& col)
{
  return dt::map_str2str(col,
    [=](size_t, dt::CString& value, dt::string_buf* sb) {
      sb->write(value == *x? *y : value);
    });
}


Column ReplaceAgent::replace_strN(dt::CString* x, dt::CString* y,
                                   const Column& col, size_t n)
{
  return dt::map_str2str(col,
    [=](size_t, dt::CString& value, dt::string_buf* sb) {
      for (size_t j = 0; j < n; ++j) {
        if (value == x[j]) {
          sb->write(y[j]);
          return;
        }
      }
      sb->write(value);
    });
}




RESTORE_CLANG_WARNING("-Wpadded")
}  // namespace py
