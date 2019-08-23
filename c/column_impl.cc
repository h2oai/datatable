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
#include "parallel/api.h"
#include "column_impl.h"




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


// TODO: replace these with ref-counting semantics

ColumnImpl* ColumnImpl::acquire_instance() const {
  return this->shallowcopy();
}

void ColumnImpl::release_instance() {
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

template <typename T>
void _materialize_fw(const ColumnImpl* input_column, ColumnImpl* output_column)
{
  using R = promote<T>;
  assert_compatible_type<R>(input_column->stype());
  xassert(input_column->nrows() == output_column->nrows());
  xassert(!output_column->is_virtual());

  auto out_data = static_cast<T*>(output_column->data_w());
  dt::parallel_for_static(
    input_column->nrows(),
    [&](size_t i) {
      R value;
      bool isna = input_column->get_element(i, &value);
      out_data[i] = isna? GETNA<T>() : static_cast<T>(value);
    });
}


ColumnImpl* ColumnImpl::materialize() {
  ColumnImpl* out = ColumnImpl::new_impl(_stype);
  out->_nrows = _nrows;
  out->init_data();
  switch (_stype) {
    case SType::BOOL:
    case SType::INT8:    _materialize_fw<int8_t> (this, out); break;
    case SType::INT16:   _materialize_fw<int16_t>(this, out); break;
    case SType::INT32:   _materialize_fw<int32_t>(this, out); break;
    case SType::INT64:   _materialize_fw<int64_t>(this, out); break;
    case SType::FLOAT32: _materialize_fw<float>  (this, out); break;
    case SType::FLOAT64: _materialize_fw<double> (this, out); break;
    default:
      throw NotImplError() << "Cannot materialize column of stype `"
                           << _stype << "`";
  }
  return out;
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

size_t ColumnImpl::data_nrows() const {
  return _nrows;
}

void ColumnImpl::init_data() {}

void ColumnImpl::resize_and_fill(size_t) {
  throw NotImplError();
}

void ColumnImpl::apply_na_mask(const Column&) {
  throw NotImplError();
}

void ColumnImpl::replace_values(Column&, const RowIndex&, const Column&) {
  throw NotImplError();
}

void ColumnImpl::fill_na_mask(int8_t*, size_t, size_t) {
  throw NotImplError();
}

void ColumnImpl::rbind_impl(colvec&, size_t, bool) {
  throw NotImplError();
}

void ColumnImpl::fill_na() {
  throw NotImplError();
}
