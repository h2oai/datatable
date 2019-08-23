//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include "csv/toa.h"
#include "expr/virtual_column.h"
#include "parallel/api.h"
#include "utils/exceptions.h"
#include "column_impl.h"  // TODO: remove
namespace dt {
namespace expr {


virtual_column::virtual_column(size_t nrows, SType stype)
  : _nrows(nrows), _stype(stype) {}

virtual_column::~virtual_column() {}


size_t virtual_column::nrows() const noexcept {
  return _nrows;
}

SType virtual_column::stype() const noexcept {
  return _stype;
}


void virtual_column::compute(size_t, int8_t*) {
  throw RuntimeError() << "int8 value cannot be computed";   // LCOV_EXCL_LINE
}

void virtual_column::compute(size_t, int16_t*) {
  throw RuntimeError() << "int16 value cannot be computed";  // LCOV_EXCL_LINE
}

void virtual_column::compute(size_t, int32_t*) {
  throw RuntimeError() << "int32 value cannot be computed";  // LCOV_EXCL_LINE
}

void virtual_column::compute(size_t, int64_t*) {
  throw RuntimeError() << "int64 value cannot be computed";  // LCOV_EXCL_LINE
}

void virtual_column::compute(size_t, float*) {
  throw RuntimeError() << "float value cannot be computed";  // LCOV_EXCL_LINE
}

void virtual_column::compute(size_t, double*) {
  throw RuntimeError() << "double value cannot be computed"; // LCOV_EXCL_LINE
}

void virtual_column::compute(size_t, CString*) {
  throw RuntimeError() << "string value cannot be computed"; // LCOV_EXCL_LINE
}



//------------------------------------------------------------------------------
// to_column
//------------------------------------------------------------------------------

template <typename T>
void materialize_fw(virtual_column* self, Column& outcol) {
  T* out_data = static_cast<T*>(outcol->data_w());
  dt::parallel_for_static(
    outcol.nrows(),
    [&](size_t i) {
      self->compute(i, out_data + i);
    });
}


Column virtual_column::to_column() {
  Column out = Column::new_data_column(_stype, _nrows);
  switch (_stype) {
    case SType::BOOL:
    case SType::INT8:    materialize_fw<int8_t> (this, out); break;
    case SType::INT16:   materialize_fw<int16_t>(this, out); break;
    case SType::INT32:   materialize_fw<int32_t>(this, out); break;
    case SType::INT64:   materialize_fw<int64_t>(this, out); break;
    case SType::FLOAT32: materialize_fw<float>  (this, out); break;
    case SType::FLOAT64: materialize_fw<double> (this, out); break;
    default:
      throw NotImplError() << "virtual_column of stype " << _stype
          << " cannot be materialized";
  }
  return out;
}



//------------------------------------------------------------------------------
// virtualize
//------------------------------------------------------------------------------

class _vcolumn : public virtual_column {
  protected:
    Column column;

  public:
    explicit _vcolumn(Column&& col);
    Column to_column() override;
};

_vcolumn::_vcolumn(Column&& col)
  : virtual_column(col.nrows(), col.stype()),
    column(std::move(col)) {}

Column _vcolumn::to_column() {
  return std::move(column);
}



template <typename T>
class fw_vcol : public _vcolumn {
  protected:
    const T* data;

  public:
    explicit fw_vcol(Column&& col)
      : _vcolumn(std::move(col)),
        data(static_cast<const T*>(column->data())) {}

    void compute(size_t i, T* out) override {
      *out = data[i];
    }
};


template <typename A, typename T>
class arr_fw_vcol : public fw_vcol<T> {
  private:
    const A* index;

  public:
    arr_fw_vcol(Column&& col, const A* indices)
      : fw_vcol<T>(std::move(col)),
        index(indices) {}

    void compute(size_t i, T* out) override {
      A j = index[i];
      *out = (j == -1) ? GETNA<T>() : this->data[j];
    }
};


template <typename T>
class slice_fw_vcol : public fw_vcol<T> {
  private:
    size_t istart;
    size_t istep;

  public:
    slice_fw_vcol(Column&& col, size_t start, size_t step)
      : fw_vcol<T>(std::move(col)),
        istart(start),
        istep(step) {}

    void compute(size_t i, T* out) override {
      size_t j = istart + i * istep;
      *out = this->data[j];
    }
};



template <typename T>
class str_vcol : public _vcolumn {
  public:
    using _vcolumn::_vcolumn;

    void compute(size_t i, CString* out) override {
      bool isna = column.get_element(i, out);
      if (isna) out->ch = nullptr;
    }
};


template <typename A, typename T>
class arr_str_vcol : public str_vcol<T> {
  private:
    const A* index;

  public:
    arr_str_vcol(Column&& col, const A* indices)
      : str_vcol<T>(std::move(col)),
        index(indices) {}

    void compute(size_t i, CString* out) override {
      A j = index[i];
      if (j == -1) {
        out->ch = nullptr;
        out->size = 0;
      } else {
        str_vcol<T>::compute(static_cast<size_t>(j), out);
      }
    }
};


template <typename T>
class slice_str_vcol : public str_vcol<T> {
  private:
    size_t istart;
    size_t istep;

  public:
    slice_str_vcol(Column&& col, size_t start, size_t step)
      : str_vcol<T>(std::move(col)),
        istart(start),
        istep(step) {}

    void compute(size_t i, CString* out) override {
      size_t j = istart + i * istep;
      str_vcol<T>::compute(j, out);
    }
};



vcolptr virtualize(Column&& col) {
  SType st = col.stype();
  const RowIndex& ri = col->rowindex();
  switch (ri.type()) {
    case RowIndexType::UNKNOWN: {
      switch (st) {
        case SType::BOOL:    return vcolptr(new fw_vcol<int8_t> (std::move(col)));
        case SType::INT8:    return vcolptr(new fw_vcol<int8_t> (std::move(col)));
        case SType::INT16:   return vcolptr(new fw_vcol<int16_t>(std::move(col)));
        case SType::INT32:   return vcolptr(new fw_vcol<int32_t>(std::move(col)));
        case SType::INT64:   return vcolptr(new fw_vcol<int64_t>(std::move(col)));
        case SType::FLOAT32: return vcolptr(new fw_vcol<float>  (std::move(col)));
        case SType::FLOAT64: return vcolptr(new fw_vcol<double> (std::move(col)));
        case SType::STR32:   return vcolptr(new str_vcol<uint32_t>(std::move(col)));
        case SType::STR64:   return vcolptr(new str_vcol<uint64_t>(std::move(col)));
        default: break;
      }
      break;
    }

    case RowIndexType::ARR32: {
      const int32_t* ind32 = ri.indices32();
      switch (st) {
        case SType::BOOL:    return vcolptr(new arr_fw_vcol<int32_t, int8_t> (std::move(col), ind32));
        case SType::INT8:    return vcolptr(new arr_fw_vcol<int32_t, int8_t> (std::move(col), ind32));
        case SType::INT16:   return vcolptr(new arr_fw_vcol<int32_t, int16_t>(std::move(col), ind32));
        case SType::INT32:   return vcolptr(new arr_fw_vcol<int32_t, int32_t>(std::move(col), ind32));
        case SType::INT64:   return vcolptr(new arr_fw_vcol<int32_t, int64_t>(std::move(col), ind32));
        case SType::FLOAT32: return vcolptr(new arr_fw_vcol<int32_t, float>  (std::move(col), ind32));
        case SType::FLOAT64: return vcolptr(new arr_fw_vcol<int32_t, double> (std::move(col), ind32));
        case SType::STR32:   return vcolptr(new arr_str_vcol<int32_t, uint32_t>(std::move(col), ind32));
        case SType::STR64:   return vcolptr(new arr_str_vcol<int32_t, uint64_t>(std::move(col), ind32));
        default: break;
      }
      break;
    }

    case RowIndexType::ARR64: {
      const int64_t* ind64 = ri.indices64();
      switch (st) {
        case SType::BOOL:    return vcolptr(new arr_fw_vcol<int64_t, int8_t> (std::move(col), ind64));
        case SType::INT8:    return vcolptr(new arr_fw_vcol<int64_t, int8_t> (std::move(col), ind64));
        case SType::INT16:   return vcolptr(new arr_fw_vcol<int64_t, int16_t>(std::move(col), ind64));
        case SType::INT32:   return vcolptr(new arr_fw_vcol<int64_t, int32_t>(std::move(col), ind64));
        case SType::INT64:   return vcolptr(new arr_fw_vcol<int64_t, int64_t>(std::move(col), ind64));
        case SType::FLOAT32: return vcolptr(new arr_fw_vcol<int64_t, float>  (std::move(col), ind64));
        case SType::FLOAT64: return vcolptr(new arr_fw_vcol<int64_t, double> (std::move(col), ind64));
        case SType::STR32:   return vcolptr(new arr_str_vcol<int64_t, uint32_t>(std::move(col), ind64));
        case SType::STR64:   return vcolptr(new arr_str_vcol<int64_t, uint64_t>(std::move(col), ind64));
        default: break;
      }
      break;
    }

    case RowIndexType::SLICE: {
      size_t start = ri.slice_start();
      size_t step = ri.slice_step();
      switch (st) {
        case SType::BOOL:    return vcolptr(new slice_fw_vcol<int8_t> (std::move(col), start, step));
        case SType::INT8:    return vcolptr(new slice_fw_vcol<int8_t> (std::move(col), start, step));
        case SType::INT16:   return vcolptr(new slice_fw_vcol<int16_t>(std::move(col), start, step));
        case SType::INT32:   return vcolptr(new slice_fw_vcol<int32_t>(std::move(col), start, step));
        case SType::INT64:   return vcolptr(new slice_fw_vcol<int64_t>(std::move(col), start, step));
        case SType::FLOAT32: return vcolptr(new slice_fw_vcol<float>  (std::move(col), start, step));
        case SType::FLOAT64: return vcolptr(new slice_fw_vcol<double> (std::move(col), start, step));
        case SType::STR32:   return vcolptr(new slice_str_vcol<uint32_t>(std::move(col), start, step));
        case SType::STR64:   return vcolptr(new slice_str_vcol<uint64_t>(std::move(col), start, step));
        default: break;
      }
      break;
    }
  }
  throw NotImplError() << "Cannot create virtual column of stype " << st;  // LCOV_EXCL_LINE
}




//------------------------------------------------------------------------------
// cast
//------------------------------------------------------------------------------

template <typename T>
class cast_fw_vcol : public virtual_column {
  private:
    vcolptr arg;

  public:
    cast_fw_vcol(vcolptr&& col, SType new_stype)
      : virtual_column(col->nrows(), new_stype),
        arg(std::move(col)) {}

    void compute(size_t i, int8_t* out) override {
      T x;
      arg->compute(i, &x);
      *out = ISNA<T>(x)? GETNA<int8_t>() : static_cast<int8_t>(x);
    }

    void compute(size_t i, int16_t* out) override {
      T x;
      arg->compute(i, &x);
      *out = ISNA<T>(x)? GETNA<int16_t>() : static_cast<int16_t>(x);
    }

    void compute(size_t i, int32_t* out) override {
      T x;
      arg->compute(i, &x);
      *out = ISNA<T>(x)? GETNA<int32_t>() : static_cast<int32_t>(x);
    }

    void compute(size_t i, int64_t* out) override {
      T x;
      arg->compute(i, &x);
      *out = ISNA<T>(x)? GETNA<int64_t>() : static_cast<int64_t>(x);
    }

    void compute(size_t i, float* out) override {
      T x;
      arg->compute(i, &x);
      *out = ISNA<T>(x)? GETNA<float>() : static_cast<float>(x);
    }

    void compute(size_t i, double* out) override {
      T x;
      arg->compute(i, &x);
      *out = ISNA<T>(x)? GETNA<double>() : static_cast<double>(x);
    }

    void compute(size_t i, CString* out) override {
      static thread_local char buffer[30];
      T x;
      arg->compute(i, &x);
      if (ISNA<T>(x)) {
        out->ch = nullptr;
      } else {
        char* ch = buffer;
        toa<T>(&ch, x);
        out->ch = buffer;
        out->size = ch - buffer;
      }
    }
};


vcolptr cast(vcolptr&& vcol, SType new_stype) {
  SType old_stype = vcol->stype();
  switch (old_stype) {
    case SType::BOOL:
    case SType::INT8:    return vcolptr(new cast_fw_vcol<int8_t> (std::move(vcol), new_stype));
    case SType::INT16:   return vcolptr(new cast_fw_vcol<int16_t>(std::move(vcol), new_stype));
    case SType::INT32:   return vcolptr(new cast_fw_vcol<int32_t>(std::move(vcol), new_stype));
    case SType::INT64:   return vcolptr(new cast_fw_vcol<int64_t>(std::move(vcol), new_stype));
    case SType::FLOAT32: return vcolptr(new cast_fw_vcol<float>  (std::move(vcol), new_stype));
    case SType::FLOAT64: return vcolptr(new cast_fw_vcol<double> (std::move(vcol), new_stype));
    default: break;
  }
  throw NotImplError() << "Cannot virtual-cast column of stype " << old_stype;
}




}}  // namespace dt::expr
