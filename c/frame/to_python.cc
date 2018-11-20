//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/args.h"
#include "python/string.h"
#include "python/tuple.h"

namespace py {


//------------------------------------------------------------------------------
// converters for various stypes
//------------------------------------------------------------------------------

class converter {
  public:
    virtual ~converter();
    virtual oobj to_oobj(int64_t row) const = 0;
};
using convptr = std::unique_ptr<converter>;

converter::~converter() {}



class bool8_converter : public converter {
  private:
    const int8_t* values;
  public:
    bool8_converter(const Column*);
    oobj to_oobj(int64_t row) const override;
};

bool8_converter::bool8_converter(const Column* col) {
  values = dynamic_cast<const BoolColumn*>(col)->elements_r();
}

oobj bool8_converter::to_oobj(int64_t row) const {
  int8_t x = values[row];
  return x == 0? py::False() : x == 1? py::True() : py::None();
}



template <typename T>
class int_converter : public converter {
  private:
    const T* values;
  public:
    int_converter(const Column*);
    oobj to_oobj(int64_t row) const override;
};

template <typename T>
int_converter<T>::int_converter(const Column* col) {
  values = dynamic_cast<const IntColumn<T>*>(col)->elements_r();
}

template <typename T>
oobj int_converter<T>::to_oobj(int64_t row) const {
  T x = values[row];
  return ISNA<T>(x)? py::None() : oint(x);
}



template <typename T>
class float_converter : public converter {
  private:
    const T* values;
  public:
    float_converter(const Column*);
    oobj to_oobj(int64_t row) const override;
};

template <typename T>
float_converter<T>::float_converter(const Column* col) {
  values = dynamic_cast<const RealColumn<T>*>(col)->elements_r();
}

template <typename T>
oobj float_converter<T>::to_oobj(int64_t row) const {
  T x = values[row];
  return ISNA<T>(x)? py::None() : ofloat(x);
}



template <typename T>
class string_converter : public converter {
  private:
    const char* strdata;
    const T* offsets;
  public:
    string_converter(const Column*);
    oobj to_oobj(int64_t row) const override;
};

template <typename T>
string_converter<T>::string_converter(const Column* col) {
  auto scol = dynamic_cast<const StringColumn<T>*>(col);
  strdata = scol->strdata();
  offsets = scol->offsets();
}

template <typename T>
oobj string_converter<T>::to_oobj(int64_t row) const {
  T end = offsets[row];
  if (ISNA<T>(end)) return py::None();
  T start = offsets[row - 1] & ~GETNA<T>();
  return ostring(strdata + start, end - start);
}



class pyobj_converter : public converter {
  private:
    const PyObject* const* values;
  public:
    pyobj_converter(const Column*);
    oobj to_oobj(int64_t row) const override;
};

pyobj_converter::pyobj_converter(const Column* col) {
  values = dynamic_cast<const PyObjectColumn*>(col)->elements_r();
}

oobj pyobj_converter::to_oobj(int64_t row) const {
  return oobj(values[row]);
}



static convptr make_converter(const Column* col) {
  SType stype = col->stype();
  switch (stype) {
    case SType::BOOL:    return convptr(new bool8_converter(col));
    case SType::INT8:    return convptr(new int_converter<int8_t>(col));
    case SType::INT16:   return convptr(new int_converter<int16_t>(col));
    case SType::INT32:   return convptr(new int_converter<int32_t>(col));
    case SType::INT64:   return convptr(new int_converter<int64_t>(col));
    case SType::FLOAT32: return convptr(new float_converter<float>(col));
    case SType::FLOAT64: return convptr(new float_converter<double>(col));
    case SType::STR32:   return convptr(new string_converter<uint32_t>(col));
    case SType::STR64:   return convptr(new string_converter<uint64_t>(col));
    case SType::OBJ:     return convptr(new pyobj_converter(col));
    default:
      throw ValueError() << "Cannot stringify column of type " << stype;
  }
}



//------------------------------------------------------------------------------
// Frame's API
//------------------------------------------------------------------------------

NoArgs Frame::Type::args_to_tuples("to_tuples",
R"(to_tuples(self)
--

Convert the Frame into a list of tuples, by rows.

Returns a list having `nrows` tuples, where each tuple has length `ncols` and
contains data from each respective row of the Frame.

Examples
--------
>>> DT = dt.Frame(A=[1, 2, 3], B=["aye", "nay", "tain"])
>>> DT.to_tuples()
[(1, "aye"), (2, "nay"), (3, "tain")]
)");


oobj Frame::to_tuples(const NoArgs&) {
  std::vector<py::otuple> list_of_tuples;
  for (size_t i = 0; i < dt->nrows; ++i) {
    list_of_tuples.push_back(py::otuple(dt->ncols));
  }
  for (size_t j = 0; j < dt->ncols; ++j) {
    const Column* col = dt->columns[j];
    const RowIndex& ri = col->rowindex();
    auto conv = make_converter(col);
    ri.strided_loop2(0, static_cast<int64_t>(dt->nrows), 1,
      [&](int64_t i, int64_t ii) {
        oobj x = ii >= 0? conv->to_oobj(ii) : py::None();
        list_of_tuples[static_cast<size_t>(i)].set(j, std::move(x));
      });
  }
  py::olist res(dt->nrows);
  for (size_t i = 0; i < dt->nrows; ++i) {
    res.set(i, std::move(list_of_tuples[i]));
  }
  return std::move(res);
}



NoArgs Frame::Type::args_to_list("to_list",
R"(to_list(self)
--

Convert the Frame into a list of lists, by columns.

Returns a list of `ncols` lists, each inner list representing one column of
the Frame.

Examples
--------
>>> DT = dt.Frame(A=[1, 2, 3], B=["aye", "nay", "tain"])
>>> DT.to_list()
[[1, 2, 3], ["aye", "nay", "tain"]]
)");

oobj Frame::to_list(const NoArgs&) {
  py::olist res(dt->ncols);
  for (size_t j = 0; j < dt->ncols; ++j) {
    py::olist pycol(dt->nrows);
    const Column* col = dt->columns[j];
    const RowIndex& ri = col->rowindex();
    auto conv = make_converter(col);
    ri.strided_loop2(0, static_cast<int64_t>(dt->nrows), 1,
      [&](int64_t i, int64_t ii) {
        oobj x = ii >= 0? conv->to_oobj(ii) : py::None();
        pycol.set(i, std::move(x));
      });
    res.set(j, std::move(pycol));
  }
  return std::move(res);
}



NoArgs Frame::Type::args_to_dict("to_dict",
R"(to_dict(self)
--

Convert the Frame into a dictionary of lists, by columns.

Returns a dictionary with `ncols` entries, each being the `colname: coldata`
pair, where `colname` is a string, and `coldata` is an array of column's data.

Examples
--------
>>> DT = dt.Frame(A=[1, 2, 3], B=["aye", "nay", "tain"])
>>> DT.to_dict()
{"A": [1, 2, 3], "B": ["aye", "nay", "tain"]}
)");

oobj Frame::to_dict(const NoArgs&) {
  py::otuple names = dt->get_pynames();
  py::odict res;
  for (size_t j = 0; j < dt->ncols; ++j) {
    py::olist pycol(dt->nrows);
    const Column* col = dt->columns[j];
    const RowIndex& ri = col->rowindex();
    auto conv = make_converter(col);
    ri.strided_loop2(0, static_cast<int64_t>(dt->nrows), 1,
      [&](int64_t i, int64_t ii) {
        oobj x = ii >= 0? conv->to_oobj(ii) : py::None();
        pycol.set(i, std::move(x));
      });
    res.set(names[j], pycol);
  }
  return std::move(res);
}



};
