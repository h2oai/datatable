//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include <Python.h>
#include "utils/omp.h"



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

BoolColumn::BoolColumn() : FwColumn<int8_t>() {}

BoolColumn::BoolColumn(int64_t nrows_, MemoryBuffer* mb) :
    FwColumn<int8_t>(nrows_, mb) {}

BoolColumn::~BoolColumn() {}


SType BoolColumn::stype() const {
  return ST_BOOLEAN_I1;
}



//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

BooleanStats* BoolColumn::get_stats() const {
  if (stats == nullptr) stats = new BooleanStats();
  return static_cast<BooleanStats*>(stats);
}

# define DEF_STAT_GET(STAT, RET_TYPE)                                           \
  RET_TYPE BoolColumn:: STAT () const {                                         \
    BooleanStats *s = get_stats();                                              \
    if (!s-> STAT ## _computed()) s->STAT ## _compute(this);                    \
    return s->STAT ## _get();                                                   \
}

DEF_STAT_GET(mean, double)
DEF_STAT_GET(sd, double)
DEF_STAT_GET(min, int8_t)
DEF_STAT_GET(max, int8_t)
DEF_STAT_GET(sum, int64_t)

#undef DEF_STAT_GET

// Retrieve stat value as a column
Column* BoolColumn::min_column() const {
  BoolColumn* col = new BoolColumn(1);
  col->set_elem(0, min());
  return col;
}

Column* BoolColumn::max_column() const {
  BoolColumn* col = new BoolColumn(1);
  col->set_elem(0, max());
  return col;
}

Column* BoolColumn::sum_column() const {
  IntColumn<int64_t>* col = new IntColumn<int64_t>(1);
  col->set_elem(0, sum());
  return col;
}

Column* BoolColumn::mean_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, mean());
  return col;
}

Column* BoolColumn::sd_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, sd());
  return col;
}

PyObject* BoolColumn::min_pyscalar() const { return bool_to_py(min()); }
PyObject* BoolColumn::max_pyscalar() const { return bool_to_py(max()); }
PyObject* BoolColumn::sum_pyscalar() const { return int_to_py(sum()); }
PyObject* BoolColumn::mean_pyscalar() const { return float_to_py(mean()); }
PyObject* BoolColumn::sd_pyscalar() const { return float_to_py(sd()); }



//------------------------------------------------------------------------------
// Type casts
//------------------------------------------------------------------------------

template<typename T>
inline static void cast_helper(int64_t nrows, const int8_t* src, T* trg) {
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < nrows; ++i) {
    int8_t x = src[i];
    trg[i] = ISNA<int8_t>(x)? GETNA<T>() : static_cast<T>(x);
  }
}

void BoolColumn::cast_into(BoolColumn* target) const {
  memcpy(target->data(), data(), alloc_size());
}

void BoolColumn::cast_into(IntColumn<int8_t>* target) const {
  memcpy(target->data(), data(), alloc_size());
}

void BoolColumn::cast_into(IntColumn<int16_t>* target) const {
  cast_helper<int16_t>(nrows, this->elements(), target->elements());
}

void BoolColumn::cast_into(IntColumn<int32_t>* target) const {
  cast_helper<int32_t>(nrows, this->elements(), target->elements());
}

void BoolColumn::cast_into(IntColumn<int64_t>* target) const {
  cast_helper<int64_t>(nrows, this->elements(), target->elements());
}

void BoolColumn::cast_into(RealColumn<float>* target) const {
  cast_helper<float>(nrows, this->elements(), target->elements());
}

void BoolColumn::cast_into(RealColumn<double>* target) const {
  cast_helper<double>(nrows, this->elements(), target->elements());
}

void BoolColumn::cast_into(PyObjectColumn* target) const {
  int8_t*    src_data = this->elements();
  PyObject** trg_data = target->elements();
  for (int64_t i = 0; i < nrows; ++i) {
    trg_data[i] = bool_to_py(src_data[i]);
  }
}



//------------------------------------------------------------------------------
// Integrity checks
//------------------------------------------------------------------------------

bool BoolColumn::verify_integrity(IntegrityCheckContext& icc,
                                  const std::string& name) const
{
  bool r = FwColumn<int8_t>::verify_integrity(icc, name);
  if (!r) return false;
  int nerrors = icc.n_errors();
  auto end = icc.end();

  // Check that all elements in column are either 0, 1, or NA_I1
  int64_t mbuf_nrows = data_nrows();
  int8_t *vals = elements();
  for (int64_t i = 0; i < mbuf_nrows; ++i) {
    int8_t val = vals[i];
    if (!(val == 0 || val == 1 || val == NA_I1)) {
      icc << "(Boolean) " << name << " has value " << val << " in row " << i
          << end;
    }
  }
  return !icc.has_errors(nerrors);
}
