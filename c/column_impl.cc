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
#include "column/sentinel_fw.h"
#include "parallel/api.h"
#include "parallel/string_utils.h"
#include "column_impl.h"



//------------------------------------------------------------------------------
// Basic constructors
//------------------------------------------------------------------------------

ColumnImpl::ColumnImpl(size_t nrows, SType stype)
  : nrows_(nrows),
    stype_(stype) {}

ColumnImpl::~ColumnImpl() {}


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

template <typename T>
ColumnImpl* ColumnImpl::_materialize_fw() {
  size_t inp_nrows = this->nrows();
  SType inp_stype = this->stype();
  assert_compatible_type<T>(inp_stype);
  ColumnImpl* output_column
      = dt::Sentinel_ColumnImpl::make_column(inp_nrows, inp_stype);
  auto out_data = static_cast<T*>(output_column->mbuf.wptr());

  dt::parallel_for_static(
    inp_nrows,
    [=](size_t i) {
      T value;
      bool isvalid = this->get_element(i, &value);
      out_data[i] = isvalid? value : GETNA<T>();
    });
  return output_column;
}


ColumnImpl* ColumnImpl::_materialize_obj() {
  size_t inp_nrows = this->nrows();
  SType inp_stype = this->stype();
  assert_compatible_type<py::robj>(inp_stype);

  ColumnImpl* output_column
      = dt::Sentinel_ColumnImpl::make_column(inp_nrows, SType::OBJ);
  auto out_data = static_cast<py::oobj*>(output_column->mbuf.wptr());

  // Writing output array as `py::oobj` will ensure that the elements
  // will be properly INCREF-ed.
  for (size_t i = 0; i < inp_nrows; ++i) {
    py::robj value;
    bool isvalid = this->get_element(i, &value);
    out_data[i] = isvalid? py::oobj(value) : py::None();
  }
  return output_column;
}


ColumnImpl* ColumnImpl::_materialize_str() {
  Column inp(const_cast<ColumnImpl*>(this));
  try {
    Column rescol = dt::map_str2str(inp,
      [=](size_t, CString& value, dt::string_buf* sb) {
        sb->write(value);
      });
    inp.release();
    return rescol.release();
  }
  catch (...) {
    // prevent this from being deleted in case materialization
    // fails.
    inp.release();
    throw;
  }
}


// TODO: fix semantics of materialization...
//
ColumnImpl* ColumnImpl::materialize() {
  const_cast<ColumnImpl*>(this)->pre_materialize_hook();
  ColumnImpl* out = nullptr;
  switch (stype_) {
    case SType::BOOL:
    case SType::INT8:    out = _materialize_fw<int8_t> (); break;
    case SType::INT16:   out = _materialize_fw<int16_t>(); break;
    case SType::INT32:   out = _materialize_fw<int32_t>(); break;
    case SType::INT64:   out = _materialize_fw<int64_t>(); break;
    case SType::FLOAT32: out = _materialize_fw<float>  (); break;
    case SType::FLOAT64: out = _materialize_fw<double> (); break;
    case SType::STR32:
    case SType::STR64:   out = _materialize_str(); break;
    case SType::OBJ:     out = _materialize_obj(); break;
    default:
      throw NotImplError() << "Cannot materialize column of stype `"
                           << stype_ << "`";
  }
  delete this;
  return out;
}




//------------------------------------------------------------------------------
// fill_na_mask()
//------------------------------------------------------------------------------

template <typename T>
void _fill_na_mask(ColumnImpl* icol, int8_t* outmask, size_t row0, size_t row1)
{
  T value;
  for (size_t i = row0; i < row1; ++i) {
    outmask[i] = !icol->get_element(i, &value);
  }
}

void ColumnImpl::fill_na_mask(int8_t* outmask, size_t row0, size_t row1) {
  switch (stype_) {
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
                           << stype_ << "`";
  }
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

void ColumnImpl::apply_na_mask(const Column&) {
  throw NotImplError() << "Method ColumnImpl::apply_na_mask() not implemented";
}

void ColumnImpl::replace_values(Column&, const RowIndex&, const Column&) {
  throw NotImplError() << "Method ColumnImpl::replace_values() not implemented";
}

void ColumnImpl::rbind_impl(colvec&, size_t, bool) {
  throw NotImplError() << "Method ColumnImpl::rbind_impl() not implemented";
}


void ColumnImpl::na_pad(size_t new_nrows, Column& out) {
  xassert(new_nrows > nrows());
  out = Column(new dt::NaFilled_ColumnImpl(std::move(out), new_nrows));
}

void ColumnImpl::truncate(size_t new_nrows, Column&) {
  xassert(new_nrows < nrows());
  nrows_ = new_nrows;
}
