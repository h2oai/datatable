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





void virtual_column::_compute(size_t, int8_t*) {
  throw RuntimeError() << "int8 value cannot be computed";
}

void virtual_column::_compute(size_t, int16_t*) {
  throw RuntimeError() << "int16 value cannot be computed";
}

void virtual_column::_compute(size_t, int32_t*) {
  throw RuntimeError() << "int32 value cannot be computed";
}

void virtual_column::_compute(size_t, int64_t*) {
  throw RuntimeError() << "int64 value cannot be computed";
}

void virtual_column::_compute(size_t, float*) {
  throw RuntimeError() << "float value cannot be computed";
}

void virtual_column::_compute(size_t, double*) {
  throw RuntimeError() << "double value cannot be computed";
}

void virtual_column::_compute(size_t, CString*) {
  throw RuntimeError() << "string value cannot be computed";
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
      self->_compute(i, out_data + i);
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

    void _compute(size_t i, T* out) override {
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

    void _compute(size_t i, T* out) override {
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

    void _compute(size_t i, T* out) override {
      size_t j = istart + i * istep;
      *out = this->data[j];
    }
};



template <typename T>
class str_vcol : public _vcolumn {
  public:
    using _vcolumn::_vcolumn;

    void _compute(size_t i, CString* out) override {
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

    void _compute(size_t i, CString* out) override {
      A j = index[i];
      if (j == -1) {
        out->ch = nullptr;
        out->size = 0;
      } else {
        str_vcol<T>::_compute(static_cast<size_t>(j), out);
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

    void _compute(size_t i, CString* out) override {
      size_t j = istart + i * istep;
      str_vcol<T>::_compute(j, out);
    }
};



vcolptr::vcolptr(Column&& col) {
  SType st = col.stype();
  const RowIndex& ri = col->rowindex();
  switch (ri.type()) {
    case RowIndexType::UNKNOWN: {
      switch (st) {
        case SType::BOOL:    vcol = new fw_vcol<int8_t> (std::move(col)); return;
        case SType::INT8:    vcol = new fw_vcol<int8_t> (std::move(col)); return;
        case SType::INT16:   vcol = new fw_vcol<int16_t>(std::move(col)); return;
        case SType::INT32:   vcol = new fw_vcol<int32_t>(std::move(col)); return;
        case SType::INT64:   vcol = new fw_vcol<int64_t>(std::move(col)); return;
        case SType::FLOAT32: vcol = new fw_vcol<float>  (std::move(col)); return;
        case SType::FLOAT64: vcol = new fw_vcol<double> (std::move(col)); return;
        case SType::STR32:   vcol = new str_vcol<uint32_t>(std::move(col)); return;
        case SType::STR64:   vcol = new str_vcol<uint64_t>(std::move(col)); return;
        default: break;
      }
      break;
    }

    case RowIndexType::ARR32: {
      const int32_t* ind32 = ri.indices32();
      switch (st) {
        case SType::BOOL:    vcol = new arr_fw_vcol<int32_t, int8_t> (std::move(col), ind32); return;
        case SType::INT8:    vcol = new arr_fw_vcol<int32_t, int8_t> (std::move(col), ind32); return;
        case SType::INT16:   vcol = new arr_fw_vcol<int32_t, int16_t>(std::move(col), ind32); return;
        case SType::INT32:   vcol = new arr_fw_vcol<int32_t, int32_t>(std::move(col), ind32); return;
        case SType::INT64:   vcol = new arr_fw_vcol<int32_t, int64_t>(std::move(col), ind32); return;
        case SType::FLOAT32: vcol = new arr_fw_vcol<int32_t, float>  (std::move(col), ind32); return;
        case SType::FLOAT64: vcol = new arr_fw_vcol<int32_t, double> (std::move(col), ind32); return;
        case SType::STR32:   vcol = new arr_str_vcol<int32_t, uint32_t>(std::move(col), ind32); return;
        case SType::STR64:   vcol = new arr_str_vcol<int32_t, uint64_t>(std::move(col), ind32); return;
        default: break;
      }
      break;
    }

    case RowIndexType::ARR64: {
      const int64_t* ind64 = ri.indices64();
      switch (st) {
        case SType::BOOL:    vcol = new arr_fw_vcol<int64_t, int8_t> (std::move(col), ind64); return;
        case SType::INT8:    vcol = new arr_fw_vcol<int64_t, int8_t> (std::move(col), ind64); return;
        case SType::INT16:   vcol = new arr_fw_vcol<int64_t, int16_t>(std::move(col), ind64); return;
        case SType::INT32:   vcol = new arr_fw_vcol<int64_t, int32_t>(std::move(col), ind64); return;
        case SType::INT64:   vcol = new arr_fw_vcol<int64_t, int64_t>(std::move(col), ind64); return;
        case SType::FLOAT32: vcol = new arr_fw_vcol<int64_t, float>  (std::move(col), ind64); return;
        case SType::FLOAT64: vcol = new arr_fw_vcol<int64_t, double> (std::move(col), ind64); return;
        case SType::STR32:   vcol = new arr_str_vcol<int64_t, uint32_t>(std::move(col), ind64); return;
        case SType::STR64:   vcol = new arr_str_vcol<int64_t, uint64_t>(std::move(col), ind64); return;
        default: break;
      }
      break;
    }

    case RowIndexType::SLICE: {
      size_t start = ri.slice_start();
      size_t step = ri.slice_step();
      switch (st) {
        case SType::BOOL:    vcol = new slice_fw_vcol<int8_t> (std::move(col), start, step); return;
        case SType::INT8:    vcol = new slice_fw_vcol<int8_t> (std::move(col), start, step); return;
        case SType::INT16:   vcol = new slice_fw_vcol<int16_t>(std::move(col), start, step); return;
        case SType::INT32:   vcol = new slice_fw_vcol<int32_t>(std::move(col), start, step); return;
        case SType::INT64:   vcol = new slice_fw_vcol<int64_t>(std::move(col), start, step); return;
        case SType::FLOAT32: vcol = new slice_fw_vcol<float>  (std::move(col), start, step); return;
        case SType::FLOAT64: vcol = new slice_fw_vcol<double> (std::move(col), start, step); return;
        case SType::STR32:   vcol = new slice_str_vcol<uint32_t>(std::move(col), start, step); return;
        case SType::STR64:   vcol = new slice_str_vcol<uint64_t>(std::move(col), start, step); return;
        default: break;
      }
      break;
    }
  }
  vcol = nullptr;
  throw NotImplError() << "Cannot create virtual column of stype " << st;
}




//------------------------------------------------------------------------------
// cast
//------------------------------------------------------------------------------

template <typename T>
class cast_fw_vcol : public virtual_column {
  private:
    vcolptr arg;

  public:
    cast_fw_vcol(vcolptr&& vcol, SType new_stype)
      : virtual_column(vcol.nrows(), new_stype),
        arg(std::move(vcol)) {}

    void _compute(size_t i, int8_t* out) override {
      T x;
      bool isna = arg.get_element(i, &x);
      *out = ISNA<T>(x)? GETNA<int8_t>() : static_cast<int8_t>(x);
    }

    void _compute(size_t i, int16_t* out) override {
      T x;
      bool isna = arg.get_element(i, &x);
      *out = ISNA<T>(x)? GETNA<int16_t>() : static_cast<int16_t>(x);
    }

    void _compute(size_t i, int32_t* out) override {
      T x;
      bool isna = arg.get_element(i, &x);
      *out = ISNA<T>(x)? GETNA<int32_t>() : static_cast<int32_t>(x);
    }

    void _compute(size_t i, int64_t* out) override {
      T x;
      bool isna = arg.get_element(i, &x);
      *out = ISNA<T>(x)? GETNA<int64_t>() : static_cast<int64_t>(x);
    }

    void _compute(size_t i, float* out) override {
      T x;
      bool isna = arg.get_element(i, &x);
      *out = ISNA<T>(x)? GETNA<float>() : static_cast<float>(x);
    }

    void _compute(size_t i, double* out) override {
      T x;
      bool isna = arg.get_element(i, &x);
      *out = ISNA<T>(x)? GETNA<double>() : static_cast<double>(x);
    }

    void _compute(size_t i, CString* out) override {
      static thread_local char buffer[30];
      T x;
      bool isna = arg.get_element(i, &x);
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


vcolptr vcolptr::cast(SType new_stype) && {
  SType old_stype = stype();
  switch (old_stype) {
    case SType::BOOL:
    case SType::INT8:    return vcolptr(new cast_fw_vcol<int8_t> (std::move(*this), new_stype));
    case SType::INT16:   return vcolptr(new cast_fw_vcol<int16_t>(std::move(*this), new_stype));
    case SType::INT32:   return vcolptr(new cast_fw_vcol<int32_t>(std::move(*this), new_stype));
    case SType::INT64:   return vcolptr(new cast_fw_vcol<int64_t>(std::move(*this), new_stype));
    case SType::FLOAT32: return vcolptr(new cast_fw_vcol<float>  (std::move(*this), new_stype));
    case SType::FLOAT64: return vcolptr(new cast_fw_vcol<double> (std::move(*this), new_stype));
    default: break;
  }
  throw NotImplError() << "Cannot virtual-cast column of stype " << old_stype;
}



//------------------------------------------------------------------------------
// vcolptr
//------------------------------------------------------------------------------

vcolptr::vcolptr() : vcol(nullptr) {}

vcolptr::vcolptr(virtual_column* v) : vcol(v) {}

vcolptr::vcolptr(vcolptr&& other) {
  vcol = other.vcol;
  other.vcol = nullptr;
}

vcolptr& vcolptr::operator=(vcolptr&& other) {
  delete vcol;
  vcol = other.vcol;
  other.vcol = nullptr;
  return *this;
}

vcolptr::~vcolptr() {
  delete vcol;
}


size_t vcolptr::nrows() const {
  return vcol->_nrows;
}

SType vcolptr::stype() const {
  return vcol->_stype;
}


bool vcolptr::get_element(size_t i, int8_t* out) {
  vcol->_compute(i, out);
  return false;
}

bool vcolptr::get_element(size_t i, int16_t* out) {
  vcol->_compute(i, out);
  return false;
}

bool vcolptr::get_element(size_t i, int32_t* out) {
  vcol->_compute(i, out);
  return false;
}

bool vcolptr::get_element(size_t i, int64_t* out) {
  vcol->_compute(i, out);
  return false;
}

bool vcolptr::get_element(size_t i, float* out) {
  vcol->_compute(i, out);
  return false;
}

bool vcolptr::get_element(size_t i, double* out) {
  vcol->_compute(i, out);
  return false;
}

bool vcolptr::get_element(size_t i, CString* out) {
  vcol->_compute(i, out);
  return false;
}




}}  // namespace dt::expr
