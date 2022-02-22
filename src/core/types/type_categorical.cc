//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include "column/categorical.h"
#include "parallel/api.h"
#include "sort.h"
#include "stype.h"
#include "types/type_invalid.h"
#include "types/type_categorical.h"
#include "utils/assert.h"

namespace dt {


//------------------------------------------------------------------------------
// Type_Cat
//------------------------------------------------------------------------------

Type_Cat::Type_Cat(SType st, Type t) : TypeImpl(st) {
  if (t.is_categorical()) {
    throw TypeError()
      << "Categories are not allowed to be of a categorical type";
  }
  elementType_ = std::move(t);
}

bool Type_Cat::is_compound() const {
  return true;
}

bool Type_Cat::is_categorical() const {
  return true;
}

bool Type_Cat::can_be_read_as_int8() const {
  return elementType_.can_be_read_as<int8_t>();
}

bool Type_Cat::can_be_read_as_int16() const {
  return elementType_.can_be_read_as<int16_t>();
}

bool Type_Cat::can_be_read_as_int32() const {
  return elementType_.can_be_read_as<int32_t>();
}

bool Type_Cat::can_be_read_as_int64() const {
  return elementType_.can_be_read_as<int64_t>();
}

bool Type_Cat::can_be_read_as_float32() const {
  return elementType_.can_be_read_as<float>();
}

bool Type_Cat::can_be_read_as_float64() const {
  return elementType_.can_be_read_as<double>();
}

bool Type_Cat::can_be_read_as_cstring() const {
  return elementType_.can_be_read_as<CString>();
}

bool Type_Cat::can_be_read_as_pyobject() const {
  return elementType_.can_be_read_as<py::oobj>();
}

bool Type_Cat::can_be_read_as_column() const {
  return elementType_.can_be_read_as<Column>();
}


TypeImpl* Type_Cat::common_type(TypeImpl* other) {
  if (other->is_categorical() &&
      elementType_ == reinterpret_cast<const Type_Cat*>(other)->elementType_)
  {
    return other->stype() > stype() ? other : this;
  }
  if (other->is_void()) {
    return this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}


bool Type_Cat::equals(const TypeImpl* other) const {
  return other->stype() == stype() &&
         elementType_ == reinterpret_cast<const Type_Cat*>(other)->elementType_;
}


size_t Type_Cat::hash() const noexcept {
  return static_cast<size_t>(stype()) + STYPES_COUNT * elementType_.hash();
}


/**
  * Cast column `col` into the categorical type.
  *
  * The following type casts are currently supported:
  *   * void    -> cat*<T>
  *   * obj64   -> cat*<T>
  */
Column Type_Cat::cast_column(Column&& col) const {
  switch (col.stype()) {
    case SType::VOID:
      return Column::new_na_column(col.nrows(), make_type());

    case SType::OBJ: {
      switch (stype()) {
        case SType::CAT8:  cast_obj_column_<uint8_t>(col); break;
        case SType::CAT16: cast_obj_column_<uint16_t>(col); break;
        case SType::CAT32: cast_obj_column_<uint32_t>(col); break;
        default: throw RuntimeError() << "Unknown categorical type";
      }
      return std::move(col);
    }

    default:
      throw NotImplError() << "Unable to cast column of type `" << col.type()
                           << "` into `" << to_string() << "`";
  }
}



/**
  * This method will create a buffer `buf` with the codes and will modify
  * `col` in-place by only leaving one element per category. It will
  * then create a categorical column from the codes and categories
  * finally assigning it to `col`.
  */
template <typename T>
void Type_Cat::cast_obj_column_(Column& col) const {
  size_t nrows = col.nrows(); // save nrows as `col` will be modified in-place

  // First, cast `col` to the requested `elementType_` and obtain
  // the categories (groups) information.
  col.cast_inplace(elementType_);
  auto res = group({col}, {SortFlag::NONE});
  RowIndex ri = std::move(res.first);
  Groupby gb = std::move(res.second);
  auto offsets = gb.offsets_r();

  Buffer buf = Buffer::mem(col.nrows() * sizeof(T));
  Buffer buf_cat = Buffer::mem(gb.size() * sizeof(int32_t));
  auto buf_ptr = static_cast<T*>(buf.xptr());
  auto buf_cat_ptr = static_cast<int32_t*>(buf_cat.xptr());

  const size_t MAX_CATS = std::numeric_limits<T>::max() + size_t(1);

  if (gb.size() > MAX_CATS) {
    throw ValueError() << "Number of categories in the column is `" << gb.size()
      << "`, that is larger than " << to_string() << " type "
      << "can accomodate, i.e. `" << MAX_CATS << "`";
  }

  // Fill out two buffers:
  // - `buf_cat` with row indices of unique elements (one element per category)
  // - `buf` with the codes of categories (group ids).
  dt::parallel_for_dynamic(gb.size(),
    [&](size_t i) {
      size_t jj;
      ri.get_element(static_cast<size_t>(offsets[i]), &jj);
      buf_cat_ptr[i] = static_cast<int32_t>(jj);

      for (int32_t j = offsets[i]; j < offsets[i + 1]; ++j) {
        ri.get_element(static_cast<size_t>(j), &jj);
        buf_ptr[static_cast<size_t>(jj)] = static_cast<T>(i);
      }
    });

  // Modify `col` in-place by only leaving one element per a category
  const RowIndex ri_cat(std::move(buf_cat), RowIndex::ARR32);
  col.apply_rowindex(ri_cat);
  col.materialize();

  size_t val_size = (nrows + 7) / 8;
  Buffer val = Buffer::mem(val_size);
  int8_t* val_data = static_cast<int8_t*>(val.xptr());
  std::memset(val_data, 255, val_size);

  // Replace `col` with the corresponding categorical column
  col = Column(new Categorical_ColumnImpl<T>(
          nrows,
          std::move(val),
          std::move(buf),
          std::move(col)
        ));
}


//------------------------------------------------------------------------------
// Type_Cat8
//------------------------------------------------------------------------------

Type_Cat8::Type_Cat8(Type t) : Type_Cat(SType::CAT8, t) {}

std::string Type_Cat8::to_string() const {
  return "cat8(" + elementType_.to_string() + ")";
}


//------------------------------------------------------------------------------
// Type_Cat16
//------------------------------------------------------------------------------

Type_Cat16::Type_Cat16(Type t) : Type_Cat(SType::CAT16, t) {}

std::string Type_Cat16::to_string() const {
  return "cat16(" + elementType_.to_string() + ")";
}

//------------------------------------------------------------------------------
// Type_Cat32
//------------------------------------------------------------------------------

Type_Cat32::Type_Cat32(Type t) : Type_Cat(SType::CAT32, std::move(t)) {}

std::string Type_Cat32::to_string() const {
  return "cat32(" + elementType_.to_string() + ")";
}


}  // namespace dt
