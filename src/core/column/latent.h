//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#ifndef dt_COLUMN_LATENT_h
#define dt_COLUMN_LATENT_h
#include "column/virtual.h"
#include "stype.h"
namespace dt {


/**
  * `Latent_ColumnImpl` is a wrapper around another virtual column
  * such that whenever the user wants to access the data of that
  * column, it will be automatically materialized.
  *
  * Thus, whenever you have a virtual column where computing each
  * element is sufficiently costly, you can wrap such column in a
  * Latent_ColumnImpl, ensuring that the data will be computed only
  * once, and only if actually needed.
  *
  * latent (adj)
  *     (of a quality or state) existing but not yet developed or
  *     manifest; hidden or concealed.
  *
  */
class Latent_ColumnImpl : public Virtual_ColumnImpl {
  private:
    mutable Column column_;
    size_t : 64;

  public:
    explicit Latent_ColumnImpl(ColumnImpl*);
    explicit Latent_ColumnImpl(const Column&);

    ColumnImpl* clone() const override;
    void materialize(Column&, bool) override;
    bool allow_parallel_access() const override;
    size_t n_children() const noexcept override;

    bool get_element(size_t, int8_t*)   const override;
    bool get_element(size_t, int16_t*)  const override;
    bool get_element(size_t, int32_t*)  const override;
    bool get_element(size_t, int64_t*)  const override;
    bool get_element(size_t, float*)    const override;
    bool get_element(size_t, double*)   const override;
    bool get_element(size_t, CString*)  const override;
    bool get_element(size_t, py::oobj*) const override;
    bool get_element(size_t, Column*)   const override;

    // The `vivify()` method must be called first in the case,
    // when the latent column's data are going to be accessed from
    // multiple threads. That's because the `materialize()`
    // method is not thread-safe.
    template <typename T>
    static void vivify(const Column& col) {
      T value;
      col.get_element(0, &value);
    }

    static void vivify(const Column& col) {
      dt::SType st = col.type().is_categorical()? col.child(0).stype()
                                                : col.stype();
      switch (st) {
        case SType::VOID:
        case SType::BOOL:
        case SType::INT8:    vivify<int8_t>(col); break;
        case SType::INT16:   vivify<int16_t>(col); break;
        case SType::DATE32:
        case SType::INT32:   vivify<int32_t>(col); break;
        case SType::TIME64:
        case SType::INT64:   vivify<int64_t>(col); break;
        case SType::FLOAT32: vivify<float>(col); break;
        case SType::FLOAT64: vivify<double>(col); break;
        case SType::STR32:
        case SType::STR64:   vivify<CString>(col); break;
        case SType::OBJ:     vivify<py::oobj>(col); break;
        case SType::ARR32:
        case SType::ARR64:   vivify<Column>(col); break;
        default:
          throw RuntimeError() << "Unknown stype " << col.stype();  // LCOV_EXCL_LINE
      }
    }


  private:
    ColumnImpl* vivify_impl(bool to_memory = false) const;
};



}  // namespace dt
#endif
