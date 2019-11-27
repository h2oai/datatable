//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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

    bool get_element(size_t, int8_t*)   const override;
    bool get_element(size_t, int16_t*)  const override;
    bool get_element(size_t, int32_t*)  const override;
    bool get_element(size_t, int64_t*)  const override;
    bool get_element(size_t, float*)    const override;
    bool get_element(size_t, double*)   const override;
    bool get_element(size_t, CString*)  const override;
    bool get_element(size_t, py::robj*) const override;

  private:
    ColumnImpl* vivify(bool to_memory = false) const;
};



}  // namespace dt
#endif
