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
#include "column/const.h"
#include "expr/head_literal.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {


Head_Literal_SliceInt::Head_Literal_SliceInt(py::oslice x)
  : value(x) {}


Kind Head_Literal_SliceInt::get_expr_kind() const {
  return Kind::SliceInt;
}



Workframe Head_Literal_SliceInt::evaluate_n(
    const vecExpr&, EvalContext&, bool) const
{
  throw TypeError() << "A slice expression cannot appear in this context";
}



Workframe Head_Literal_SliceInt::evaluate_f(
    EvalContext& ctx, size_t frame_id, bool) const
{
  size_t len = ctx.get_datatable(frame_id)->ncols();
  size_t start, count, step;
  value.normalize(len, &start, &count, &step);
  Workframe outputs(ctx);
  for (size_t i = 0; i < count; ++i) {
    outputs.add_ref_column(frame_id, start + i * step);
  }
  return outputs;
}


Workframe Head_Literal_SliceInt::evaluate_j(
    const vecExpr&, EvalContext& ctx, bool allow_new) const
{
  return evaluate_f(ctx, 0, allow_new);
}


RowIndex Head_Literal_SliceInt::evaluate_i(const vecExpr&, EvalContext& ctx) const {
  size_t len = ctx.nrows();
  size_t start, count, step;
  value.normalize(len, &start, &count, &step);
  return RowIndex(start, count, step);
}


static size_t _estimate_iby_nrows(size_t nrows, size_t ngroups,
                                  int64_t istart, int64_t istop, int64_t istep)
{
  (void) istart;
  // if (istep < 0) istep = -istep;
  // if (istep == 1) {
  //   if (istart == py::oslice::NA) istart = 0;
  //   if (istop  == py::oslice::NA) istop = static_cast<int64_t>(nrows);
  //   return std::min(nrows, ngroups * static_cast<size_t>(istop - istart));
  // }
  if (istep == 0) {
    return ngroups * static_cast<size_t>(istop);
  }
  return nrows;
}


RiGb Head_Literal_SliceInt::evaluate_iby(const vecExpr&, EvalContext& ctx) const {
  int64_t istart = value.start();
  int64_t istop = value.stop();
  int64_t istep = value.step();
  if (istep == py::oslice::NA) istep = 1;

  const Groupby& gb = ctx.get_groupby();
  size_t ngroups = gb.size();
  const int32_t* group_offsets = gb.offsets_r() + 1;
  size_t ri_size = _estimate_iby_nrows(ctx.nrows(), ngroups, istart, istop, istep);
  arr32_t out_ri_array(ri_size);

  Buffer out_groups = Buffer::mem((ngroups + 1) * sizeof(int32_t));
  int32_t* out_rowindices = out_ri_array.data();
  int32_t* out_offsets = static_cast<int32_t*>(out_groups.xptr()) + 1;
  out_offsets[-1] = 0;
  size_t j = 0;  // Counter for the row indices
  size_t k = 0;  // Counter for the number of groups written

  int32_t step = static_cast<int32_t>(istep);
  if (step > 0) {
    if (istart == py::oslice::NA) istart = 0;
    if (istop == py::oslice::NA) istop = static_cast<int64_t>(ctx.nrows());
    for (size_t g = 0; g < ngroups; ++g) {
      auto off0 = group_offsets[g - 1];
      auto off1 = group_offsets[g];
      auto group_size = off1 - off0;
      auto start = static_cast<int32_t>(istart);
      auto stop  = static_cast<int32_t>(istop);
      if (start < 0) start += group_size;
      if (start < 0) start = 0;
      start += off0;
      xassert(start >= off0);
      if (stop < 0) stop += group_size;
      stop += off0;
      if (stop > off1) stop = off1;
      if (start < stop) {
        for (int32_t i = start; i < stop; i += step) {
          out_rowindices[j++] = i;
        }
        out_offsets[k++] = static_cast<int32_t>(j);
      }
    }
  }
  else if (step < 0) {
    for (size_t g = 0; g < ngroups; ++g) {
      int32_t off0 = group_offsets[g - 1];
      int32_t off1 = group_offsets[g];
      int32_t n = off1 - off0;
      int32_t start, stop;
      start = istart == py::oslice::NA || istart >= n
              ? n - 1 : static_cast<int32_t>(istart);
      if (start < 0) start += n;
      start += off0;
      if (istop == py::oslice::NA) {
        stop = off0 - 1;
      } else {
        stop = static_cast<int32_t>(istop);
        if (stop < 0) stop += n;
        if (stop < 0) stop = -1;
        stop += off0;
      }
      if (start > stop) {
        for (int32_t i = start; i > stop; i += step) {
          out_rowindices[j++] = i;
        }
        out_offsets[k++] = static_cast<int32_t>(j);
      }
    }
  }
  else {
    xassert(istep == 0);
    xassert(istart != py::oslice::NA);
    xassert(istop != py::oslice::NA && istop > 0);
    for (size_t g = 0; g < ngroups; ++g) {
      int32_t off0 = group_offsets[g - 1];
      int32_t off1 = group_offsets[g];
      int32_t n = off1 - off0;
      int32_t start = static_cast<int32_t>(istart);
      if (start < 0) start += n;
      if (start < 0 || start >= n) continue;
      start += off0;
      for (int t = 0; t < istop; ++t) {
        out_rowindices[j++] = start;
      }
      out_offsets[k++] = static_cast<int32_t>(j);
    }
  }

  xassert(j <= ri_size);
  out_ri_array.resize(j);
  out_groups.resize((k + 1) * sizeof(int32_t));
  return std::make_pair(
            RowIndex(std::move(out_ri_array), /* sorted = */ (step >= 0)),
            Groupby(k, std::move(out_groups))
          );
}




}}  // namespace dt::expr
