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
#include "column/cast.h"
#include "column/column_impl.h"
#include "column/nafilled.h"
#include "column/sentinel_fw.h"
#include "column/truncated.h"
#include "parallel/api.h"
#include "parallel/string_utils.h"
#include "utils/macros.h"
namespace dt {



//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------

ColumnImpl::ColumnImpl(size_t nrows, SType stype)
  : nrows_(nrows),
    stype_(stype),
    refcount_(1) {}




//------------------------------------------------------------------------------
// Data access
//------------------------------------------------------------------------------

[[noreturn]] static void err(SType col_stype, const char* type) {
  throw NotImplError()
      << "Cannot retrieve " << type
      << " values from a column of type " << col_stype;
}

bool ColumnImpl::get_element(size_t, int8_t*)  const { err(stype_, "int8"); }
bool ColumnImpl::get_element(size_t, int16_t*) const { err(stype_, "int16"); }
bool ColumnImpl::get_element(size_t, int32_t*) const { err(stype_, "int32"); }
bool ColumnImpl::get_element(size_t, int64_t*) const { err(stype_, "int64"); }
bool ColumnImpl::get_element(size_t, float*)   const { err(stype_, "float32"); }
bool ColumnImpl::get_element(size_t, double*)  const { err(stype_, "float64"); }
bool ColumnImpl::get_element(size_t, CString*) const { err(stype_, "string"); }
bool ColumnImpl::get_element(size_t, py::oobj*)const { err(stype_, "object"); }




//------------------------------------------------------------------------------
// Materialization
//------------------------------------------------------------------------------

template <typename T>
void ColumnImpl::_materialize_fw(Column& out) {
  xassert(compatible_type<T>(stype_));
  auto out_column = Sentinel_ColumnImpl::make_column(nrows_, stype_);
  auto out_data = static_cast<T*>(out_column.get_data_editable(0));
  auto nthreads = NThreads(this->allow_parallel_access());

  if (computationally_expensive()) {
    parallel_for_dynamic(nrows_, nthreads,
      [=](size_t i) {
        T value;
        bool isvalid = this->get_element(i, &value);
        out_data[i] = isvalid? value : GETNA<T>();
      });
  }
  else {
    parallel_for_static(nrows_, nthreads,
      [=](size_t i) {
        T value;
        bool isvalid = this->get_element(i, &value);
        out_data[i] = isvalid? value : GETNA<T>();
      });
  }
  out = std::move(out_column);
}


void ColumnImpl::_materialize_obj(Column& out) {
  xassert(stype_ == SType::OBJ);
  auto out_column = Sentinel_ColumnImpl::make_column(nrows_, stype_);
  auto out_data = static_cast<py::oobj*>(out_column.get_data_editable(0));

  // Treating output array as `py::oobj[]` will ensure that the elements
  // will be properly INCREF-ed.
  for (size_t i = 0; i < nrows_; ++i) {
    py::oobj value;
    bool isvalid = this->get_element(i, &value);
    out_data[i] = isvalid? std::move(value) : py::None();
  }
  out = std::move(out_column);
}


void ColumnImpl::_materialize_str(Column& out) {
  out = map_str2str(out,
    [=](size_t, CString& value, string_buf* sb) {
      sb->write(value);
    });
}


void ColumnImpl::materialize(Column& out, bool to_memory) {
  (void) to_memory;  // default materialization is always to memory
  this->pre_materialize_hook();
  switch (stype_) {
    case SType::VOID:    stype_ = dt::SType::BOOL; FALLTHROUGH;
    case SType::BOOL:
    case SType::INT8:    return _materialize_fw<int8_t> (out);
    case SType::INT16:   return _materialize_fw<int16_t>(out);
    case SType::INT32:   return _materialize_fw<int32_t>(out);
    case SType::INT64:   return _materialize_fw<int64_t>(out);
    case SType::FLOAT32: return _materialize_fw<float>  (out);
    case SType::FLOAT64: return _materialize_fw<double> (out);
    case SType::STR32:
    case SType::STR64:   return _materialize_str(out);
    case SType::OBJ:     return _materialize_obj(out);
    default:
      throw NotImplError() << "Cannot materialize column of stype `"
                           << stype_ << "`";
  }
}


bool ColumnImpl::allow_parallel_access() const {
  size_t n = n_children();
  for (size_t i = 0; i < n; ++i) {
    if (!child(i).allow_parallel_access()) return false;
  }
  return true;
}



//------------------------------------------------------------------------------
// fill_npmask()
//------------------------------------------------------------------------------

template <typename T>
void ColumnImpl::_fill_npmask(bool* outmask, size_t row0, size_t row1) const {
  T value;
  for (size_t i = row0; i < row1; ++i) {
    outmask[i] = !get_element(i, &value);
  }
}

void ColumnImpl::fill_npmask(bool* outmask, size_t row0, size_t row1) const {
  if (stats_ && stats_->is_computed(Stat::NaCount) && stats_->nacount() == 0) {
    std::fill(outmask + row0, outmask + row1, false);
    return;
  }
  switch (stype_) {
    case SType::VOID: {
      std::fill(outmask + row0, outmask + row1, true);
      break;
    }
    case SType::BOOL:
    case SType::INT8:    _fill_npmask<int8_t> (outmask, row0, row1); break;
    case SType::INT16:   _fill_npmask<int16_t>(outmask, row0, row1); break;
    case SType::INT32:   _fill_npmask<int32_t>(outmask, row0, row1); break;
    case SType::INT64:   _fill_npmask<int64_t>(outmask, row0, row1); break;
    case SType::FLOAT32: _fill_npmask<float>  (outmask, row0, row1); break;
    case SType::FLOAT64: _fill_npmask<double> (outmask, row0, row1); break;
    case SType::STR32:
    case SType::STR64:   _fill_npmask<CString>(outmask, row0, row1); break;
    case SType::OBJ:     _fill_npmask<py::oobj>(outmask, row0, row1); break;
    default:
      throw NotImplError() << "Cannot fill_npmask() on column of stype `"
                           << stype_ << "`";
  }
}




//------------------------------------------------------------------------------
// Casts
//------------------------------------------------------------------------------

bool ColumnImpl::cast_const(SType new_stype, Column& thiscol) const {
  if (new_stype == SType::BOOL) {
    switch (stype()) {
      case SType::VOID:    thiscol = Column::new_na_column(nrows_, new_stype); break;
      case SType::INT8:    thiscol = Column(new CastNumericToBool_ColumnImpl<int8_t>(std::move(thiscol))); break;
      case SType::INT16:   thiscol = Column(new CastNumericToBool_ColumnImpl<int16_t>(std::move(thiscol))); break;
      case SType::INT32:   thiscol = Column(new CastNumericToBool_ColumnImpl<int32_t>(std::move(thiscol))); break;
      case SType::INT64:   thiscol = Column(new CastNumericToBool_ColumnImpl<int64_t>(std::move(thiscol))); break;
      case SType::FLOAT32: thiscol = Column(new CastNumericToBool_ColumnImpl<float>(std::move(thiscol))); break;
      case SType::FLOAT64: thiscol = Column(new CastNumericToBool_ColumnImpl<double>(std::move(thiscol))); break;
      case SType::STR32:
      case SType::STR64:   thiscol = Column(new CastStringToBool_ColumnImpl(std::move(thiscol))); break;
      case SType::OBJ:     thiscol = Column(new CastObjToBool_ColumnImpl(std::move(thiscol))); break;
      default:  throw NotImplError() << "Unable to cast column of type `" << stype() << "` into `bool8`";
    }
  }
  else {
    switch (stype()) {
      case SType::VOID:    thiscol = Column::new_na_column(nrows_, new_stype); break;
      case SType::BOOL:    thiscol = Column(new CastBool_ColumnImpl(new_stype, std::move(thiscol))); break;
      case SType::INT8:    thiscol = Column(new CastNumeric_ColumnImpl<int8_t>(new_stype, std::move(thiscol))); break;
      case SType::INT16:   thiscol = Column(new CastNumeric_ColumnImpl<int16_t>(new_stype, std::move(thiscol))); break;
      case SType::INT32:   thiscol = Column(new CastNumeric_ColumnImpl<int32_t>(new_stype, std::move(thiscol))); break;
      case SType::INT64:   thiscol = Column(new CastNumeric_ColumnImpl<int64_t>(new_stype, std::move(thiscol))); break;
      case SType::FLOAT32: thiscol = Column(new CastNumeric_ColumnImpl<float>(new_stype, std::move(thiscol))); break;
      case SType::FLOAT64: thiscol = Column(new CastNumeric_ColumnImpl<double>(new_stype, std::move(thiscol))); break;
      case SType::STR32:
      case SType::STR64:   thiscol = Column(new CastString_ColumnImpl(new_stype, std::move(thiscol))); break;
      case SType::OBJ:     thiscol = Column(new CastObject_ColumnImpl(new_stype, std::move(thiscol))); break;
      default:  throw NotImplError() << "Unable to cast column of type `" << stype() << "` into `" << new_stype << "`";
    }
  }
  return true;
}

void ColumnImpl::cast_mutate(SType) {}




//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

void ColumnImpl::replace_values(const RowIndex&, const Column&, Column&) {
  throw NotImplError() << "Method ColumnImpl::replace_values() not implemented";
}

void ColumnImpl::rbind_impl(colvec&, size_t, bool, SType&) {
  throw NotImplError() << "Method ColumnImpl::rbind_impl() not implemented";
}

const Column& ColumnImpl::child(size_t) const {
  throw RuntimeError() << "This Column object has no children";
}

void ColumnImpl::na_pad(size_t new_nrows, Column& out) {
  xassert(new_nrows > nrows());
  out = Column(new NaFilled_ColumnImpl(std::move(out), new_nrows));
}

void ColumnImpl::truncate(size_t new_nrows, Column& out) {
  xassert(new_nrows < nrows());
  out = Column(new Truncated_ColumnImpl(std::move(out), new_nrows));
}





}  // namespace dt
