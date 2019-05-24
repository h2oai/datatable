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
#include "expr/virtual_column.h"
#include "parallel/api.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


virtual_column::~virtual_column() {}



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
// materialize
//------------------------------------------------------------------------------

template <typename T>
void materialize_fw(virtual_column* self, Column* outcol) {
  xassert(self->nrows() == outcol->nrows);
  T* out_data = static_cast<T*>(outcol->data_w());
  dt::parallel_for_static(
    outcol->nrows,
    [&](size_t i) {
      self->compute(i, out_data + i);
    });
}


colptr virtual_column::materialize() {
  SType out_stype = stype();
  Column* out = Column::new_data_column(out_stype, nrows());
  colptr out_colptr(out);
  switch (out_stype) {
    case SType::BOOL:
    case SType::INT8:    materialize_fw<int8_t> (this, out); break;
    case SType::INT16:   materialize_fw<int16_t>(this, out); break;
    case SType::INT32:   materialize_fw<int32_t>(this, out); break;
    case SType::INT64:   materialize_fw<int64_t>(this, out); break;
    case SType::FLOAT32: materialize_fw<float>  (this, out); break;
    case SType::FLOAT64: materialize_fw<double> (this, out); break;
    default:
      throw NotImplError() << "virtual_column of stype " << out_stype
          << " cannot be materialized";
  }
  return out_colptr;
}



//------------------------------------------------------------------------------
// virtualize
//------------------------------------------------------------------------------

class _vcolumn : public virtual_column {
  protected:
    colptr column;

  public:
    _vcolumn(colptr&& col);
    SType stype() const override;
    size_t nrows() const override;
    colptr materialize() override;
};

_vcolumn::_vcolumn(colptr&& col) : column(std::move(col)) {}
SType _vcolumn::stype() const { return column->stype(); }
size_t _vcolumn::nrows() const { return column->nrows; }
colptr _vcolumn::materialize() { return std::move(column); }



template <typename T>
class fw_vcol : public _vcolumn {
  protected:
    const T* data;

  public:
    fw_vcol(colptr&& col)
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
    arr_fw_vcol(colptr&& col, const A* indices)
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
    slice_fw_vcol(colptr&& col, size_t start, size_t step)
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
  protected:
    const T* offsets;
    const char* strdata;

  public:
    str_vcol(colptr&& col)
      : _vcolumn(std::move(col)),
        offsets(static_cast<StringColumn<T>*>(column.get())->offsets()),
        strdata(static_cast<StringColumn<T>*>(column.get())->strdata()) {}

    void compute(size_t i, CString* out) override {
      T end = offsets[i];
      T start = offsets[i - 1] & ~GETNA<T>();
      out->size = static_cast<int64_t>(end - start);
      out->ch = ISNA<T>(end) ? nullptr : strdata + start;
    }
};


template <typename A, typename T>
class arr_str_vcol : public str_vcol<T> {
  private:
    const A* index;

  public:
    arr_str_vcol(colptr&& col, const A* indices)
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
    slice_str_vcol(colptr&& col, size_t start, size_t step)
      : str_vcol<T>(std::move(col)),
        istart(start),
        istep(step) {}

    void compute(size_t i, CString* out) override {
      size_t j = istart + i * istep;
      str_vcol<T>::compute(j, out);
    }
};



vcolptr virtualize(colptr&& col) {
  SType st = col->stype();
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



}}  // namespace dt::expr
