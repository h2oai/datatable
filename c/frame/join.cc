//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include <limits>       // std::numeric_limits
#include <memory>       // std::unique_ptr
#include <type_traits>  // std::is_integral
#include <vector>       // std::vector
#include "parallel/api.h"
#include "python/args.h"
#include "python/obj.h"
#include "python/tuple.h"
#include "utils/assert.h"
#include "column.h"
#include "datatable.h"
#include "datatablemodule.h"
#include "types.h"

class Cmp;
using cmpptr = std::unique_ptr<Cmp>;
using comparator_maker = cmpptr (*)(const Column&, const Column&);
static comparator_maker cmps[DT_STYPES_COUNT][DT_STYPES_COUNT];

static cmpptr _make_comparatorM(const DataTable& Xdt, const DataTable& Jdt,
                                const intvec& x_ind, const intvec& j_ind);

static cmpptr _make_comparator1(const DataTable& Xdt, const DataTable& Jdt,
                                size_t xi, size_t ji)
{
  const Column& colx = Xdt.get_column(xi);
  const Column& colj = Jdt.get_column(ji);
  SType stype1 = colx.stype();
  SType stype2 = colj.stype();
  auto cmp = cmps[static_cast<size_t>(stype1)][static_cast<size_t>(stype2)];
  if (!cmp) {
    throw TypeError() << "Column `" << Xdt.get_names()[xi] << "` of type "
        << stype1 << " in the left Frame cannot be joined to column `"
        << Jdt.get_names()[ji] << "` of incompatible type " << stype2
        << " in the right Frame";
  }
  return cmp(colx, colj);
}

static cmpptr _make_comparator(const DataTable& Xdt, const DataTable& Jdt,
                               const intvec& x_indices, const intvec& j_indices)
{
  xassert(x_indices.size() == j_indices.size());
  if (x_indices.size() == 1) {
    return _make_comparator1(Xdt, Jdt, x_indices[0], j_indices[0]);
  } else {
    return _make_comparatorM(Xdt, Jdt, x_indices, j_indices);
  }
}



//------------------------------------------------------------------------------
// Cmp
//------------------------------------------------------------------------------

/**
 * Abstract base class that facilitates comparison of rows between two frames,
 * called X and J. The frames have different roles: J is the "look up" frame,
 * and values within this frame are assumed sorted; X is the "main" frame, and
 * we will be looking up the values from this Frame in J.
 *
 * The implementation classes are expected to store references to both frames,
 * and then provide the following comparison functions:
 *
 *   set_xrow(size_t row) -> int
 *     This selects a row within the X frame. All subsequent comparisons
 *     will be done against that row.
 *     The function returns 0 if it succeeds, or -1 if the requested row will
 *     not be able to match any row in the J frame.
 *
 *   cmp_jrow(size_t row) -> int
 *     compare the `row`th value in the J frame against the value from the
 *     X frame stored during the previous `set_xrow()` call. Returns +1, -1,
 *     or 0 depending if the row-value is greater than, less than, or equal
 *     to the value stored.
 *
 * This comparison function is then used as the basis for the binary search
 * algorithm to perform a join between two tables.
 */
class Cmp {
  public:
    virtual ~Cmp();
    virtual int cmp_jrow(size_t row) const = 0;
    virtual int set_xrow(size_t row) = 0;
};

Cmp::~Cmp() {}



//------------------------------------------------------------------------------
// MultiCmp
//------------------------------------------------------------------------------

class MultiCmp : public Cmp {
  private:
    std::vector<cmpptr> col_cmps;

  public:
    MultiCmp(const DataTable& Xdt, const DataTable& Jdt,
             const intvec& Xindices, const intvec& Jindices);
    int set_xrow(size_t row) override;
    int cmp_jrow(size_t row) const override;
};

static cmpptr _make_comparatorM(const DataTable& Xdt, const DataTable& Jdt,
                                const intvec& x_ind, const intvec& j_ind) {
  return cmpptr(new MultiCmp(Xdt, Jdt, x_ind, j_ind));
}


MultiCmp::MultiCmp(const DataTable& Xdt, const DataTable& Jdt,
                   const intvec& Xindices, const intvec& Jindices)
{
  xassert(Xindices.size() == Jindices.size());
  for (size_t i = 0; i < Xindices.size(); ++i) {
    size_t xi = Xindices[i];
    size_t ji = Jindices[i];
    col_cmps.push_back(_make_comparator1(Xdt, Jdt, xi, ji));
  }
}

int MultiCmp::set_xrow(size_t row) {
  int r = 0;
  for (cmpptr& ch : col_cmps) {
    r |= ch->set_xrow(row);
  }
  return r;
}

int MultiCmp::cmp_jrow(size_t row) const {
  for (const cmpptr& ch : col_cmps) {
    int r = ch->cmp_jrow(row);
    if (r) return r;
  }
  return 0;
}



//------------------------------------------------------------------------------
// Fixed-width Cmp
//------------------------------------------------------------------------------

template <typename TX, typename TJ>
class FwCmp : public Cmp {
  private:
    const Column& colX;
    const Column& colJ;
    TJ x_value;   // Current value from X frame, converted to TJ type
    bool x_valid;
    size_t : (64 - 8 * sizeof(TJ) - 8) & 63;

  public:
    FwCmp(const Column&, const Column&);
    static cmpptr make(const Column&, const Column&);

    int cmp_jrow(size_t row) const override;
    int set_xrow(size_t row) override;
};


template <typename TX, typename TJ>
FwCmp<TX, TJ>::FwCmp(const Column& xcol, const Column& jcol)
  : colX(xcol), colJ(jcol)
{
  assert_compatible_type<TX>(xcol.stype());
  assert_compatible_type<TJ>(jcol.stype());
}

template <typename TX, typename TJ>
cmpptr FwCmp<TX, TJ>::make(const Column& col1, const Column& col2) {
  return cmpptr(new FwCmp<TX, TJ>(col1, col2));
}


template <typename TX, typename TJ>
int FwCmp<TX, TJ>::cmp_jrow(size_t row) const {
  TJ j_value;
  bool j_valid = colJ.get_element(row, &j_value);
  if (j_valid && x_valid) {
    return (j_value > x_value) - (j_value < x_value);
  } else {
    return j_valid - x_valid;
  }
}


template <typename TX, typename TJ>
int FwCmp<TX, TJ>::set_xrow(size_t row) {
  TX newval;
  x_valid = colX.get_element(row, &newval);
  if (x_valid) {
    x_value = static_cast<TJ>(newval);
    if (std::is_integral<TJ>::value) {
      if (std::is_integral<TX>::value &&
          sizeof(TX) > sizeof(TJ) &&
          (newval > static_cast<TX>(std::numeric_limits<TJ>::max()) ||
           newval < static_cast<TX>(std::numeric_limits<TJ>::min())))
        return -1;
      // If matching floating point value to an integer column, values that
      // are not round numbers should not match.
      if (!std::is_integral<TX>::value &&
          static_cast<TX>(x_value) != newval)
        return -1;
    }
  } else {
    // x_value can be left intact
  }
  return 0;
}



//------------------------------------------------------------------------------
// String Cmp
//------------------------------------------------------------------------------

class StringCmp : public Cmp {
  private:
    const Column& colX;
    const Column& colJ;
    CString x_value;

  public:
    StringCmp(const Column&, const Column&);
    static cmpptr make(const Column&, const Column&);

    int cmp_jrow(size_t row) const override;
    int set_xrow(size_t row) override;
};


StringCmp::StringCmp(const Column& xcol, const Column& jcol)
  : colX(xcol), colJ(jcol) {}

cmpptr StringCmp::make(const Column& col1, const Column& col2) {
  return cmpptr(new StringCmp(col1, col2));
}


int StringCmp::cmp_jrow(size_t row) const {
  CString j_value;
  bool j_valid = colJ.get_element(row, &j_value);
  bool x_valid = !x_value.isna();
  if (!(j_valid && x_valid)) return j_valid - x_valid;

  int64_t xlen = x_value.size;
  int64_t jlen = j_value.size;
  const char* xstr = x_value.ch;
  const char* jstr = j_value.ch;
  for (int64_t i = 0; i < jlen; ++i) {
    if (i == xlen) return 1;  // jstr is longer than xstr
    char jch = jstr[i];
    char xch = xstr[i];
    if (xch != jch) {
      return 1 - 2*(static_cast<uint8_t>(jch) < static_cast<uint8_t>(xch));
    }
  }
  return -(jlen != xlen);
}


int StringCmp::set_xrow(size_t row) {
  bool isvalid = colX.get_element(row, &x_value);
  if (!isvalid) x_value.ch = nullptr;
  return 0;
}



//------------------------------------------------------------------------------
// Comparators for different stypes
//------------------------------------------------------------------------------

static void _init_comparators() {
  for (size_t i = 0; i < DT_STYPES_COUNT; ++i) {
    for (size_t j = 0; j < DT_STYPES_COUNT; ++j) {
      cmps[i][j] = nullptr;
    }
  }
  size_t bool8 = static_cast<size_t>(SType::BOOL);
  size_t int08 = static_cast<size_t>(SType::INT8);
  size_t int16 = static_cast<size_t>(SType::INT16);
  size_t int32 = static_cast<size_t>(SType::INT32);
  size_t int64 = static_cast<size_t>(SType::INT64);
  size_t flt32 = static_cast<size_t>(SType::FLOAT32);
  size_t flt64 = static_cast<size_t>(SType::FLOAT64);
  size_t str32 = static_cast<size_t>(SType::STR32);
  size_t str64 = static_cast<size_t>(SType::STR64);
  cmps[bool8][bool8] = FwCmp<int8_t, int8_t>::make;
  cmps[bool8][int08] = FwCmp<int8_t, int8_t>::make;
  cmps[bool8][int16] = FwCmp<int8_t, int16_t>::make;
  cmps[bool8][int32] = FwCmp<int8_t, int32_t>::make;
  cmps[bool8][int64] = FwCmp<int8_t, int64_t>::make;
  cmps[bool8][flt32] = FwCmp<int8_t, float>::make;
  cmps[bool8][flt64] = FwCmp<int8_t, double>::make;
  cmps[int08][bool8] = FwCmp<int8_t, int8_t>::make;
  cmps[int08][int08] = FwCmp<int8_t, int8_t>::make;
  cmps[int08][int16] = FwCmp<int8_t, int16_t>::make;
  cmps[int08][int32] = FwCmp<int8_t, int32_t>::make;
  cmps[int08][int64] = FwCmp<int8_t, int64_t>::make;
  cmps[int08][flt32] = FwCmp<int8_t, float>::make;
  cmps[int08][flt64] = FwCmp<int8_t, double>::make;
  cmps[int16][bool8] = FwCmp<int16_t, int8_t>::make;
  cmps[int16][int08] = FwCmp<int16_t, int8_t>::make;
  cmps[int16][int16] = FwCmp<int16_t, int16_t>::make;
  cmps[int16][int32] = FwCmp<int16_t, int32_t>::make;
  cmps[int16][int64] = FwCmp<int16_t, int64_t>::make;
  cmps[int16][flt32] = FwCmp<int16_t, float>::make;
  cmps[int16][flt64] = FwCmp<int16_t, double>::make;
  cmps[int32][bool8] = FwCmp<int32_t, int8_t>::make;
  cmps[int32][int08] = FwCmp<int32_t, int8_t>::make;
  cmps[int32][int16] = FwCmp<int32_t, int16_t>::make;
  cmps[int32][int32] = FwCmp<int32_t, int32_t>::make;
  cmps[int32][int64] = FwCmp<int32_t, int64_t>::make;
  cmps[int32][flt32] = FwCmp<int32_t, float>::make;
  cmps[int32][flt64] = FwCmp<int32_t, double>::make;
  cmps[int64][bool8] = FwCmp<int64_t, int8_t>::make;
  cmps[int64][int08] = FwCmp<int64_t, int8_t>::make;
  cmps[int64][int16] = FwCmp<int64_t, int16_t>::make;
  cmps[int64][int32] = FwCmp<int64_t, int32_t>::make;
  cmps[int64][int64] = FwCmp<int64_t, int64_t>::make;
  cmps[int64][flt32] = FwCmp<int64_t, float>::make;
  cmps[int64][flt64] = FwCmp<int64_t, double>::make;
  cmps[flt32][bool8] = FwCmp<float, int8_t>::make;
  cmps[flt32][int08] = FwCmp<float, int8_t>::make;
  cmps[flt32][int16] = FwCmp<float, int16_t>::make;
  cmps[flt32][int32] = FwCmp<float, int32_t>::make;
  cmps[flt32][int64] = FwCmp<float, int64_t>::make;
  cmps[flt32][flt32] = FwCmp<float, float>::make;
  cmps[flt32][flt64] = FwCmp<float, double>::make;
  cmps[flt64][bool8] = FwCmp<double, int8_t>::make;
  cmps[flt64][int08] = FwCmp<double, int8_t>::make;
  cmps[flt64][int16] = FwCmp<double, int16_t>::make;
  cmps[flt64][int32] = FwCmp<double, int32_t>::make;
  cmps[flt64][int64] = FwCmp<double, int64_t>::make;
  cmps[flt64][flt32] = FwCmp<double, float>::make;
  cmps[flt64][flt64] = FwCmp<double, double>::make;
  cmps[str32][str32] = StringCmp::make;
  cmps[str32][str64] = StringCmp::make;
  cmps[str64][str32] = StringCmp::make;
  cmps[str64][str64] = StringCmp::make;
}



//------------------------------------------------------------------------------
// Join functionality
//------------------------------------------------------------------------------

static size_t binsearch(Cmp* cmp, size_t nrows) {
  // Use unsigned indices in order to avoid overflows
  size_t start = 0;
  size_t end   = nrows - 1;
  while (start < end) {
    // We won't ever have as many rows for this to overflow
    size_t mid = (start + end) >> 1;
    int r = cmp->cmp_jrow(mid);
    // Note: `mid` can be 0 (if start == 0 and end == 1), so don't use
    // `mid - 1` here.
    if (r > 0) end = mid;
    else if (r < 0) start = mid + 1;
    else return mid;
  }
  return cmp->cmp_jrow(start) == 0? start : size_t(RowIndex::NA_ARR32);
}



// declared in datatable.h
RowIndex natural_join(const DataTable& xdt, const DataTable& jdt) {
  size_t k = jdt.nkeys();  // Number of join columns
  xassert(k > 0);

  // Determine how key columns in `jdt` match the columns in `xdt`
  intvec xcols, jcols;
  py::otuple jnames = jdt.get_pynames();
  for (size_t i = 0; i < k; ++i) {
    int64_t index = xdt.colindex(jnames[i]);
    if (index == -1) {
      throw ValueError() << "Key column `" << jnames[i].to_string() << "` does "
          "not exist in the left Frame";
    }
    xcols.push_back(static_cast<size_t>(index));
    jcols.push_back(i);
  }

  arr32_t arr_result_indices(xdt.nrows());
  if (xdt.nrows()) {
    int32_t* result_indices = arr_result_indices.data();
    size_t nchunks = std::min(std::max(xdt.nrows() / 200, size_t(1)),
                              dt::num_threads_in_pool());
    xassert(nchunks);

    if (jdt.nrows() == 0) {
      dt::parallel_for_static(xdt.nrows(),
        [&](size_t i) {
          result_indices[i] = RowIndex::NA_ARR32;
        });
    }
    else {
      dt::parallel_region(dt::NThreads(nchunks),
        [&] {
          // Creating the comparator may fail if xcols and jcols are incompatible
          cmpptr comparator = _make_comparator(xdt, jdt, xcols, jcols);

          dt::nested_for_static(xdt.nrows(),
            [&](size_t i) {
              int r = comparator->set_xrow(i);
              if (r == 0) {
                size_t j = binsearch(comparator.get(), jdt.nrows());
                result_indices[i] = static_cast<int32_t>(j);
              } else {
                result_indices[i] = RowIndex::NA_ARR32;
              }
            });
        });
    }
  }

  return RowIndex(std::move(arr_result_indices));
}



void py::DatatableModule::init_methods_join() {
  _init_comparators();
}
