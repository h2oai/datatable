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
#include <limits>
#include <memory>
#include <type_traits>
#include "column.h"
#include "datatable.h"
#include "datatablemodule.h"
#include "options.h"
#include "py_rowindex.h"
#include "python/args.h"
#include "python/obj.h"
#include "python/tuple.h"
#include "types.h"
#include "utils/assert.h"

class Cmp;
using cmpptr = std::unique_ptr<Cmp>;
using comparator_maker = cmpptr (*)(const Column*, const Column*);
static cmpptr make_comparator(const Column* col1, const Column* col2);



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
    MultiCmp(const colvec& Xcols, const colvec& Jcols);
    int set_xrow(size_t row) override;
    int cmp_jrow(size_t row) const override;
};


MultiCmp::MultiCmp(const colvec& Xcols, const colvec& Jcols) {
  xassert(Xcols.size() == Jcols.size());
  for (size_t i = 0; i < Xcols.size(); ++i) {
    col_cmps.push_back(make_comparator(Xcols[i], Jcols[i]));
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
    const TX* dataX;
    const TJ* dataJ;
    TJ x_value;  // Current value from X frame, converted to TJ type
    size_t : (64 - 8 * sizeof(TJ)) & 63;

  public:
    FwCmp(const Column*, const Column*);
    static cmpptr make(const Column*, const Column*);

    int cmp_jrow(size_t row) const override;
    int set_xrow(size_t row) override;
};


template <typename TX, typename TJ>
FwCmp<TX, TJ>::FwCmp(const Column* xcol, const Column* jcol) {
  auto xcol_f = dynamic_cast<const FwColumn<TX>*>(xcol);
  auto jcol_f = dynamic_cast<const FwColumn<TJ>*>(jcol);
  xassert(xcol_f && jcol_f);
  dataX = xcol_f->elements_r();
  dataJ = jcol_f->elements_r();
}

template <typename TX, typename TJ>
cmpptr FwCmp<TX, TJ>::make(const Column* col1, const Column* col2) {
  return cmpptr(new FwCmp<TX, TJ>(col1, col2));
}


template <typename TX, typename TJ>
int FwCmp<TX, TJ>::cmp_jrow(size_t row) const {
  TJ jval = dataJ[row];
  return (jval > x_value) - (jval < x_value);
}


template <typename TX, typename TJ>
int FwCmp<TX, TJ>::set_xrow(size_t row) {
  TX newval = dataX[row];
  if (ISNA<TX>(newval)) {
    x_value = GETNA<TJ>();
  } else {
    x_value = static_cast<TJ>(newval);
    if (std::is_integral<TJ>::value) {
      if (std::is_integral<TX>::value &&
          sizeof(TX) > sizeof(TJ) &&
          (newval > static_cast<TX>(std::numeric_limits<TJ>::max()) ||
           newval < static_cast<TX>(std::numeric_limits<TJ>::min())))
        return -1;
      // If matching floating point value to an integer column, values that
      // are not round numbers should not match. We return `lt` comparator
      // in this case, which is not entirely accurate, but it will provide
      // the desired no-match semantics.
      if (!std::is_integral<TX>::value &&
          static_cast<TX>(x_value) != newval)
        return -1;
    }
  }
  return 0;
}



//------------------------------------------------------------------------------
// String Cmp
//------------------------------------------------------------------------------

template <typename TX, typename TJ>
class StringCmp : public Cmp {
  private:
    const uint8_t* strdataX;
    const uint8_t* strdataJ;
    const TX* offsetsX;
    const TJ* offsetsJ;
    TX xstart;
    TX xend;
    size_t : (128 - 2 * 8 * sizeof(TJ)) & 63;

  public:
    StringCmp(const Column*, const Column*);
    static cmpptr make(const Column*, const Column*);

    int cmp_jrow(size_t row) const override;
    int set_xrow(size_t row) override;
};


template <typename TX, typename TJ>
StringCmp<TX, TJ>::StringCmp(const Column* xcol, const Column* jcol) {
  auto xcol_s = dynamic_cast<const StringColumn<TX>*>(xcol);
  auto jcol_s = dynamic_cast<const StringColumn<TJ>*>(jcol);
  xassert(xcol_s && jcol_s);
  strdataX = xcol_s->ustrdata();
  offsetsX = xcol_s->offsets();
  strdataJ = jcol_s->ustrdata();
  offsetsJ = jcol_s->offsets();
}

template <typename TX, typename TJ>
cmpptr StringCmp<TX, TJ>::make(const Column* col1, const Column* col2) {
  return cmpptr(new StringCmp<TX, TJ>(col1, col2));
}


template <typename TX, typename TJ>
int StringCmp<TX, TJ>::cmp_jrow(size_t row) const {
  TJ jend = offsetsJ[row];
  if (ISNA<TJ>(jend)) return ISNA<TX>(xend) - 1;
  if (ISNA<TX>(xend)) return 1;

  TJ jstart = offsetsJ[row - 1] & ~GETNA<TJ>();
  TJ jlen = jend - jstart;
  TX xlen = xend - xstart;
  const uint8_t* xstr = strdataX + xstart;
  const uint8_t* jstr = strdataJ + jstart;
  for (TJ i = 0; i < jlen; ++i) {
    if (i == xlen) return 1;  // jstr is shorter than xstr
    uint8_t ch1 = xstr[i];
    uint8_t ch2 = jstr[i];
    if (ch1 != ch2) {
      return 1 - 2*(ch1 < ch2);
    }
  }
  return -(jlen != xlen);
}


template <typename TX, typename TJ>
int StringCmp<TX, TJ>::set_xrow(size_t row) {
  xend   = offsetsX[row];
  xstart = offsetsX[row - 1] & ~GETNA<TX>();
  return 0;
}



//------------------------------------------------------------------------------
// Cmp factory function
//------------------------------------------------------------------------------
static comparator_maker cmps[DT_STYPES_COUNT][DT_STYPES_COUNT];

static void _init_comparators() {
  for (size_t i = 0; i < DT_STYPES_COUNT; ++i) {
    for (size_t j = 0; j < DT_STYPES_COUNT; ++j) {
      cmps[i][j] = nullptr;
    }
  }
  size_t int08 = static_cast<size_t>(SType::INT8);
  size_t int16 = static_cast<size_t>(SType::INT16);
  size_t int32 = static_cast<size_t>(SType::INT32);
  size_t int64 = static_cast<size_t>(SType::INT64);
  size_t flt32 = static_cast<size_t>(SType::FLOAT32);
  size_t flt64 = static_cast<size_t>(SType::FLOAT64);
  size_t str32 = static_cast<size_t>(SType::STR32);
  size_t str64 = static_cast<size_t>(SType::STR64);
  cmps[int08][int08] = FwCmp<int8_t, int8_t>::make;
  cmps[int08][int16] = FwCmp<int8_t, int16_t>::make;
  cmps[int08][int32] = FwCmp<int8_t, int32_t>::make;
  cmps[int08][int64] = FwCmp<int8_t, int64_t>::make;
  cmps[int08][flt32] = FwCmp<int8_t, float>::make;
  cmps[int08][flt64] = FwCmp<int8_t, double>::make;
  cmps[int16][int08] = FwCmp<int16_t, int8_t>::make;
  cmps[int16][int16] = FwCmp<int16_t, int16_t>::make;
  cmps[int16][int32] = FwCmp<int16_t, int32_t>::make;
  cmps[int16][int64] = FwCmp<int16_t, int64_t>::make;
  cmps[int16][flt32] = FwCmp<int16_t, float>::make;
  cmps[int16][flt64] = FwCmp<int16_t, double>::make;
  cmps[int32][int08] = FwCmp<int32_t, int8_t>::make;
  cmps[int32][int16] = FwCmp<int32_t, int16_t>::make;
  cmps[int32][int32] = FwCmp<int32_t, int32_t>::make;
  cmps[int32][int64] = FwCmp<int32_t, int64_t>::make;
  cmps[int32][flt32] = FwCmp<int32_t, float>::make;
  cmps[int32][flt64] = FwCmp<int32_t, double>::make;
  cmps[int64][int08] = FwCmp<int64_t, int8_t>::make;
  cmps[int64][int16] = FwCmp<int64_t, int16_t>::make;
  cmps[int64][int32] = FwCmp<int64_t, int32_t>::make;
  cmps[int64][int64] = FwCmp<int64_t, int64_t>::make;
  cmps[int64][flt32] = FwCmp<int64_t, float>::make;
  cmps[int64][flt64] = FwCmp<int64_t, double>::make;
  cmps[flt32][int08] = FwCmp<float, int8_t>::make;
  cmps[flt32][int16] = FwCmp<float, int16_t>::make;
  cmps[flt32][int32] = FwCmp<float, int32_t>::make;
  cmps[flt32][int64] = FwCmp<float, int64_t>::make;
  cmps[flt32][flt32] = FwCmp<float, float>::make;
  cmps[flt32][flt64] = FwCmp<float, double>::make;
  cmps[flt64][int08] = FwCmp<double, int8_t>::make;
  cmps[flt64][int16] = FwCmp<double, int16_t>::make;
  cmps[flt64][int32] = FwCmp<double, int32_t>::make;
  cmps[flt64][int64] = FwCmp<double, int64_t>::make;
  cmps[flt64][flt32] = FwCmp<double, float>::make;
  cmps[flt64][flt64] = FwCmp<double, double>::make;
  cmps[str32][str32] = StringCmp<uint32_t, uint32_t>::make;
  cmps[str32][str64] = StringCmp<uint32_t, uint64_t>::make;
  cmps[str64][str32] = StringCmp<uint64_t, uint32_t>::make;
  cmps[str64][str64] = StringCmp<uint64_t, uint64_t>::make;
}


static cmpptr make_comparator(const Column* col1, const Column* col2) {
  size_t st1 = static_cast<size_t>(col1->stype());
  size_t st2 = static_cast<size_t>(col2->stype());
  if (cmps[st1][st2] == nullptr) {
    throw TypeError() << "Incompatible column types: "
        << col1->stype() << " and " << col2->stype();
  }
  return cmps[st1][st2](col1, col2);
}




//------------------------------------------------------------------------------
// Join functionality
//------------------------------------------------------------------------------
static constexpr size_t NO_MATCH = size_t(-1);

static size_t binsearch(Cmp* cmp, size_t nrows) {
  // Use unsigned indices in order to avoid overflows
  size_t start = 0;
  size_t end   = nrows - 1;
  while (start < end) {
    // We won't ever have as many rows for this to overflow
    size_t mid = (start + end) >> 1;
    int r = cmp->cmp_jrow(mid);
    if (r > 0) end = mid - 1;
    else if (r < 0) start = mid + 1;
    else return mid;
  }
  return cmp->cmp_jrow(start) == 0? start : NO_MATCH;
}



static py::PKArgs fn_natural_join(
    2, 0, 0,
    false, false,
    {"xdt", "jdt"},
    "natural_join",
R"(natural_join(xdt, jdt)
--

Join two Frames `xdt` and `jdt` on the keys of `jdt`.
)",

[](const py::PKArgs& args) -> py::oobj {
  DataTable* xdt = args[0].to_frame();
  DataTable* jdt = args[1].to_frame();
  size_t k = jdt->get_nkeys();  // Number of join columns
  xassert(k > 0);

  colvec xcols, jcols;
  py::otuple jnames = jdt->get_pynames();
  for (size_t i = 0; i < k; ++i) {
    int64_t index = xdt->colindex(jnames[i]);
    if (index == -1) {
      throw ValueError() << "Key column `" << jnames[i].to_string() << "` does "
          "not exist in the left Frame";
    }
    xcols.push_back(xdt->columns[static_cast<size_t>(index)]);
    jcols.push_back(jdt->columns[i]);
  }

  arr32_t arr_result_indices(xdt->nrows);
  int32_t* result_indices = arr_result_indices.data();

  size_t nchunks = std::min(xdt->nrows / 2,
                            static_cast<size_t>(config::nthreads));

  // #pragma omp parallel num_threads(nchunks)
  {
    MultiCmp comparator(xcols, jcols);

    // #pragma omp parallel for
    for (size_t i = 0; i < xdt->nrows; ++i) {
      comparator.set_xrow(i);
      size_t j = binsearch(&comparator, jdt->nrows);
      result_indices[i] = (j == NO_MATCH)? GETNA<int32_t>()
                                         : static_cast<int32_t>(j);
    }
  }

  RowIndex res = RowIndex::from_array32(std::move(arr_result_indices));
  return py::oobj::from_new_reference(pyrowindex::wrap(res));
});


void DatatableModule::init_methods_join() {
  _init_comparators();
  ADDFN(fn_natural_join);
}
