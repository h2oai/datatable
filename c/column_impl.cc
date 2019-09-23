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
#include "column/nafilled.h"
#include "parallel/api.h"
#include "parallel/string_utils.h"
#include "column_impl.h"



ColumnImpl::ColumnImpl(size_t nrows, SType stype)
  : _nrows(nrows),
    _stype(stype) {}

ColumnImpl::~ColumnImpl() {}


ColumnImpl* ColumnImpl::new_impl(SType stype) {
  switch (stype) {
    case SType::VOID:    return new VoidColumn();
    case SType::BOOL:    return new BoolColumn();
    case SType::INT8:    return new IntColumn<int8_t>();
    case SType::INT16:   return new IntColumn<int16_t>();
    case SType::INT32:   return new IntColumn<int32_t>();
    case SType::INT64:   return new IntColumn<int64_t>();
    case SType::FLOAT32: return new FwColumn<float>();
    case SType::FLOAT64: return new FwColumn<double>();
    case SType::STR32:   return new StringColumn<uint32_t>();
    case SType::STR64:   return new StringColumn<uint64_t>();
    case SType::OBJ:     return new PyObjectColumn();
    default:
      throw ValueError()
          << "Unable to create a column of stype `" << stype << "`";
  }
}

ColumnImpl* ColumnImpl::new_impl(SType stype, size_t nrows) {
  auto ret = new_impl(stype);
  ret->_nrows = nrows;
  ret->init_data();
  return ret;
}


ColumnImpl* ColumnImpl::new_impl(void* addr, SType stype) {
  switch (stype) {
    case SType::VOID:    return new (addr) VoidColumn();
    case SType::BOOL:    return new (addr) BoolColumn();
    case SType::INT8:    return new (addr) IntColumn<int8_t>();
    case SType::INT16:   return new (addr) IntColumn<int16_t>();
    case SType::INT32:   return new (addr) IntColumn<int32_t>();
    case SType::INT64:   return new (addr) IntColumn<int64_t>();
    case SType::FLOAT32: return new (addr) FwColumn<float>();
    case SType::FLOAT64: return new (addr) FwColumn<double>();
    case SType::STR32:   return new (addr) StringColumn<uint32_t>();
    case SType::STR64:   return new (addr) StringColumn<uint64_t>();
    case SType::OBJ:     return new (addr) PyObjectColumn();
    default:
      throw ValueError()
          << "Unable to create a column of stype `" << stype << "`";
  }
}


// TODO: replace these with ref-counting semantics

ColumnImpl* ColumnImpl::acquire_instance() const {
  return this->shallowcopy();
}

void ColumnImpl::release_instance() noexcept {
  delete this;
}




//------------------------------------------------------------------------------
// Data access
//------------------------------------------------------------------------------

[[noreturn]] static void _notimpl(const ColumnImpl* col, const char* type) {
  throw NotImplError()
      << "Cannot retrieve " << type
      << " values from a column of type " << col->stype();
}

bool ColumnImpl::get_element(size_t, int8_t*)   const { _notimpl(this, "int8"); }
bool ColumnImpl::get_element(size_t, int16_t*)  const { _notimpl(this, "int16"); }
bool ColumnImpl::get_element(size_t, int32_t*)  const { _notimpl(this, "int32"); }
bool ColumnImpl::get_element(size_t, int64_t*)  const { _notimpl(this, "int64"); }
bool ColumnImpl::get_element(size_t, float*)    const { _notimpl(this, "float32"); }
bool ColumnImpl::get_element(size_t, double*)   const { _notimpl(this, "float64"); }
bool ColumnImpl::get_element(size_t, CString*)  const { _notimpl(this, "string"); }
bool ColumnImpl::get_element(size_t, py::robj*) const { _notimpl(this, "object"); }




//------------------------------------------------------------------------------
// Materialization
//------------------------------------------------------------------------------
template<> inline py::oobj GETNA() { return py::None(); }


template <typename T>
void _materialize_fw(const ColumnImpl* input_column, ColumnImpl** pout)
{
  using U = typename std::conditional<std::is_same<T, py::robj>::value,
                                      py::oobj, T>::type;
  SType inp_stype = input_column->stype();
  assert_compatible_type<T>(inp_stype);
  ColumnImpl* output_column = *pout;
  if (!output_column) {
    output_column = ColumnImpl::new_impl(inp_stype, input_column->nrows());
    *pout = output_column;
  }

  // TODO: instead we should check whether the input is thread-safe
  auto nthreads = (inp_stype == SType::OBJ)? dt::NThreads(1)
                                           : dt::NThreads();

  auto out_data = static_cast<U*>(output_column->data_w());
  dt::parallel_for_static(
    input_column->nrows(),
    nthreads,
    [&](size_t i) {
      T value;
      bool isna = input_column->get_element(i, &value);
      out_data[i] = isna? GETNA<U>() : static_cast<U>(value);  // TODO: store NA separately
    });
}


static void _materialize_str(const ColumnImpl* input_column, ColumnImpl** pout)
{
  Column inp(const_cast<ColumnImpl*>(input_column));
  try {
    Column rescol = dt::map_str2str(inp,
      [=](size_t, CString& value, dt::string_buf* sb) {
        sb->write(value);
      });
    *pout = rescol.release();
  } catch (...) {
    // prevent input_column from being deleted in case materialization
    // fails.
    inp.release();
    throw;
  }
  inp.release();
}


// TODO: fix semantics of materialization...
//
ColumnImpl* ColumnImpl::materialize() {
  const_cast<ColumnImpl*>(this)->pre_materialize_hook();
  ColumnImpl* out = nullptr;
  switch (_stype) {
    case SType::BOOL:
    case SType::INT8:    _materialize_fw<int8_t> (this, &out); break;
    case SType::INT16:   _materialize_fw<int16_t>(this, &out); break;
    case SType::INT32:   _materialize_fw<int32_t>(this, &out); break;
    case SType::INT64:   _materialize_fw<int64_t>(this, &out); break;
    case SType::FLOAT32: _materialize_fw<float>  (this, &out); break;
    case SType::FLOAT64: _materialize_fw<double> (this, &out); break;
    case SType::OBJ:     _materialize_fw<py::robj>(this, &out); break;
    case SType::STR32:
    case SType::STR64:   _materialize_str(this, &out); break;
    default:
      throw NotImplError() << "Cannot materialize column of stype `"
                           << _stype << "`";
  }
  delete this;
  return out;
}


void ColumnImpl::materialize_at(void* addr) const {
  const_cast<ColumnImpl*>(this)->pre_materialize_hook();
  ColumnImpl* out = ColumnImpl::new_impl(addr, _stype);
  xassert(static_cast<void*>(out) == addr);
  out->_nrows = _nrows;
  // Even if this throws, `addr` will still contain an object in some
  // valid state, which is safe to destruct.
  out->init_data();
  switch (_stype) {
    case SType::BOOL:
    case SType::INT8:    _materialize_fw<int8_t> (this, &out); break;
    case SType::INT16:   _materialize_fw<int16_t>(this, &out); break;
    case SType::INT32:   _materialize_fw<int32_t>(this, &out); break;
    case SType::INT64:   _materialize_fw<int64_t>(this, &out); break;
    case SType::FLOAT32: _materialize_fw<float>  (this, &out); break;
    case SType::FLOAT64: _materialize_fw<double> (this, &out); break;
    default:
      throw NotImplError() << "Cannot materialize column of stype `"
                           << _stype << "`";
  }
}



//------------------------------------------------------------------------------
// fill_na_mask()
//------------------------------------------------------------------------------

template <typename T>
void _fill_na_mask(ColumnImpl* icol, int8_t* outmask, size_t row0, size_t row1)
{
  T value;
  for (size_t i = row0; i < row1; ++i) {
    outmask[i] = icol->get_element(i, &value);
  }
}

void ColumnImpl::fill_na_mask(int8_t* outmask, size_t row0, size_t row1) {
  switch (_stype) {
    case SType::BOOL:
    case SType::INT8:    _fill_na_mask<int8_t> (this, outmask, row0, row1); break;
    case SType::INT16:   _fill_na_mask<int16_t>(this, outmask, row0, row1); break;
    case SType::INT32:   _fill_na_mask<int32_t>(this, outmask, row0, row1); break;
    case SType::INT64:   _fill_na_mask<int64_t>(this, outmask, row0, row1); break;
    case SType::FLOAT32: _fill_na_mask<float>  (this, outmask, row0, row1); break;
    case SType::FLOAT64: _fill_na_mask<double> (this, outmask, row0, row1); break;
    case SType::STR32:
    case SType::STR64:   _fill_na_mask<CString>(this, outmask, row0, row1); break;
    default:
      throw NotImplError() << "Cannot fill_na_mask() on column of stype `"
                           << _stype << "`";
  }
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

size_t ColumnImpl::data_nrows() const {
  return _nrows;
}

void ColumnImpl::init_data() {}


void ColumnImpl::apply_na_mask(const Column&) {
  throw NotImplError() << "Method ColumnImpl::apply_na_mask() not implemented";
}

void ColumnImpl::replace_values(Column&, const RowIndex&, const Column&) {
  throw NotImplError() << "Method ColumnImpl::replace_values() not implemented";
}

void ColumnImpl::rbind_impl(colvec&, size_t, bool) {
  throw NotImplError() << "Method ColumnImpl::rbind_impl() not implemented";
}

void ColumnImpl::fill_na() {
  throw NotImplError() << "Method ColumnImpl::fill_na() not implemented";
}


void ColumnImpl::na_pad(size_t new_nrows, bool inplace, Column& out) {
  (void) inplace;
  xassert(new_nrows > nrows());
  out = Column(new dt::NaFilled_ColumnImpl(std::move(out), new_nrows));
}

void ColumnImpl::truncate(size_t new_nrows, bool inplace, Column& out) {
  xassert(new_nrows < nrows());
  if (inplace) {
    _nrows = new_nrows;
  }
  else {
    ColumnImpl* copy = shallowcopy();
    copy->_nrows = new_nrows;
    out = Column(copy);
  }
}
