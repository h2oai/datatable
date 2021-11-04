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
#include "column/rbound.h"
#include "ltype.h"
#include "stype.h"
namespace dt {


//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------

static size_t compute_nrows(const colvec& columns) {
  size_t sum_nrows = 0;
  for (const auto& col : columns) {
    sum_nrows += col.nrows();
  }
  return sum_nrows;
}

// TODO: need better mechanism for upcasting
static SType compute_stype(const colvec& columns) {
  SType stype0 = SType::VOID;
  for (const auto& col : columns) {
    stype0 = common_stype(stype0, col.stype());
  }
  return stype0;
}


Rbound_ColumnImpl::Rbound_ColumnImpl(const colvec& columns)
  : Virtual_ColumnImpl(compute_nrows(columns), compute_stype(columns)),
    chunks_(columns)
{
  xassert(!chunks_.empty());
  for (auto& col : chunks_) {
    col.cast_inplace(stype());  // noop if stypes are the same
  }
  calculate_nacount();
  switch (stype_to_ltype(stype())) {
    case LType::BOOL: calculate_boolean_stats(); break;
    case LType::INT:  calculate_integer_stats(); break;
    default: break;
  }
}




//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

void Rbound_ColumnImpl::calculate_nacount() {
  bool is_valid = true;
  size_t na_count = 0;
  for (const auto& col : chunks_) {
    auto chunk_stats = col.get_stats_if_exist();
    if (!chunk_stats) return;
    na_count += chunk_stats->nacount(&is_valid);
    if (!is_valid) return;
  }
  stats()->set_nacount(na_count);
}


void Rbound_ColumnImpl::calculate_boolean_stats() {
  xassert(stype() == SType::BOOL);
  bool is_valid = true;
  size_t count1 = 0;
  for (const auto& col : chunks_) {
    auto chunk_stats = dynamic_cast<BooleanStats*>(col.get_stats_if_exist());
    if (!chunk_stats) return;
    double sum = chunk_stats->sum(&is_valid);
    xassert(sum == static_cast<double>(static_cast<size_t>(sum)));
    count1 += static_cast<size_t>(sum);
    if (!is_valid) return;
  }
  size_t count0 = nrows_ - count1 - stats()->nacount(&is_valid);
  auto bstats = dynamic_cast<BooleanStats*>(stats());
  xassert(is_valid && bstats);
  bstats->set_all_stats(count0, count1);
}


void Rbound_ColumnImpl::calculate_integer_stats() {
  xassert(stype() == SType::INT8 || stype() == SType::INT16 ||
          stype() == SType::INT32 || stype() == SType::INT64);
  int64_t min = std::numeric_limits<int64_t>::max();
  int64_t max = -std::numeric_limits<int64_t>::max();
  bool valid = false;
  for (const auto& col : chunks_) {
    auto stats = col.get_stats_if_exist();
    if (!stats) return;
    bool cvalid;
    int64_t cmin = stats->min_int(&cvalid);
    int64_t cmax = stats->max_int(&cvalid);
    if (cvalid) {
      if (cmin < min) min = cmin;
      if (cmax > max) max = cmax;
      valid = true;
    }
  }
  stats()->set_min(min, valid);
  stats()->set_max(max, valid);
}


void Rbound_ColumnImpl::calculate_float_stats() {
  xassert(stype() == SType::FLOAT32 || stype() == SType::FLOAT64);
  double min = std::numeric_limits<double>::infinity();
  double max = -std::numeric_limits<double>::infinity();
  bool valid = false;
  for (const auto& col : chunks_) {
    auto stats = col.get_stats_if_exist();
    if (!stats) return;
    bool cvalid;
    double cmin = stats->min_double(&cvalid);
    double cmax = stats->max_double(&cvalid);
    if (cvalid) {
      if (cmin < min) min = cmin;
      if (cmax > max) max = cmax;
      valid = true;
    }
  }
  stats()->set_min(min, valid);
  stats()->set_max(max, valid);
}




//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

ColumnImpl* Rbound_ColumnImpl::clone() const {
  auto res = new Rbound_ColumnImpl(chunks_);
  res->nrows_ = nrows_;
  return res;
}


size_t Rbound_ColumnImpl::n_children() const noexcept {
  return chunks_.size();
}


const Column& Rbound_ColumnImpl::child(size_t i) const {
  xassert(i < chunks_.size());
  return chunks_[i];
}




//------------------------------------------------------------------------------
// Data access
//------------------------------------------------------------------------------

template <typename T>
static inline bool _get(const colvec& columns, size_t i, T* out) {
  for (const auto& col : columns) {
    if (i < col.nrows()) {
      return col.get_element(i, out);
    }
    i -= col.nrows();
  }
  throw ValueError() << "Index " << i << " is out of range";
}

bool Rbound_ColumnImpl::get_element(size_t i, int8_t* out)   const { return _get(chunks_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, int16_t* out)  const { return _get(chunks_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, int32_t* out)  const { return _get(chunks_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, int64_t* out)  const { return _get(chunks_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, float* out)    const { return _get(chunks_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, double* out)   const { return _get(chunks_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, CString* out)  const { return _get(chunks_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, py::oobj* out) const { return _get(chunks_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, Column* out)   const { return _get(chunks_, i, out); }




//------------------------------------------------------------------------------
// Materialization
//------------------------------------------------------------------------------

// ColumnImpl* Rbound_ColumnImpl::materialize() {

// }


}  // namespace dt
