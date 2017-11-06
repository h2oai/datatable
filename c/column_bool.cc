//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "column.h"
#include <Python.h>
#include "utils/omp.h"

BoolColumn::BoolColumn() : FwColumn<int8_t>() {}

BoolColumn::BoolColumn(int64_t nrows_, MemoryBuffer* mb) :
    FwColumn<int8_t>(nrows_, mb) {}


BoolColumn::~BoolColumn() {}


SType BoolColumn::stype() const {
  return ST_BOOLEAN_I1;
}

//---- Stats -------------------------------------------------------------------

BooleanStats* BoolColumn::get_stats() const {
  if (stats == nullptr) stats = new BooleanStats();
  return static_cast<BooleanStats*>(stats);
}

# define DEF_STAT_GET(STAT, RET_TYPE)                                           \
  RET_TYPE BoolColumn:: STAT () const {                                         \
    BooleanStats *s = get_stats();                                              \
    if (!s-> STAT ## _computed()) s->compute_ ## STAT(this);                    \
    return s->_ ## STAT;                                                        \
}

DEF_STAT_GET(mean, double)
DEF_STAT_GET(sd, double)
DEF_STAT_GET(min, int8_t)
DEF_STAT_GET(max, int8_t)
DEF_STAT_GET(sum, int64_t)

#undef DEF_STAT_GET

// Retrieve stat value as a column
Column* BoolColumn::min_column() const {
  BoolColumn* col =
      static_cast<BoolColumn*>(new_data_column(ST_BOOLEAN_I1, 1));
  col->set_elem(0, min());
  return col;
}

Column* BoolColumn::max_column() const {
  BoolColumn* col =
      static_cast<BoolColumn*>(new_data_column(ST_BOOLEAN_I1, 1));
  col->set_elem(0, max());
  return col;
}

Column* BoolColumn::sum_column() const {
  IntColumn<int64_t>* col =
      static_cast<IntColumn<int64_t>*>(new_data_column(ST_INTEGER_I8, 1));
  col->set_elem(0, sum());
  return col;
}

Column* BoolColumn::mean_column() const {
  RealColumn<double>* col =
      static_cast<RealColumn<double>*>(new_data_column(ST_REAL_F8, 1));
  col->set_elem(0, mean());
  return col;
}

Column* BoolColumn::sd_column() const {
  RealColumn<double>* col =
      static_cast<RealColumn<double>*>(new_data_column(ST_REAL_F8, 1));
  col->set_elem(0, sd());
  return col;
}


//----- Type casts -------------------------------------------------------------

void BoolColumn::cast_into(BoolColumn* target) const {
  memcpy(target->data(), data(), alloc_size());
}

void BoolColumn::cast_into(IntColumn<int8_t>* target) const {
  memcpy(target->data(), data(), alloc_size());
}

void BoolColumn::cast_into(IntColumn<int16_t>* target) const {
  const int8_t na = GETNA<int8_t>();
  int8_t*  src_data = this->elements();
  int16_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < nrows; ++i) {
    int8_t x = src_data[i];
    trg_data[i] = x == na? GETNA<int16_t>() : x;
  }
}

void BoolColumn::cast_into(IntColumn<int32_t>* target) const {
  const int8_t na = GETNA<int8_t>();
  int8_t*  src_data = this->elements();
  int32_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < nrows; ++i) {
    int8_t x = src_data[i];
    trg_data[i] = x == na? GETNA<int32_t>() : x;
  }
}

void BoolColumn::cast_into(IntColumn<int64_t>* target) const {
  const int8_t na = GETNA<int8_t>();
  int8_t*  src_data = this->elements();
  int64_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < nrows; ++i) {
    int8_t x = src_data[i];
    trg_data[i] = x == na? GETNA<int64_t>() : x;
  }
}

void BoolColumn::cast_into(RealColumn<float>* target) const {
  const int8_t na = GETNA<int8_t>();
  int8_t* src_data = this->elements();
  float*  trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < nrows; ++i) {
    int8_t x = src_data[i];
    trg_data[i] = x == na? GETNA<float>() : x;
  }
}

void BoolColumn::cast_into(RealColumn<double>* target) const {
  const int8_t na = GETNA<int8_t>();
  int8_t* src_data = this->elements();
  double* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < nrows; ++i) {
    int8_t x = src_data[i];
    trg_data[i] = x == na? GETNA<double>() : x;
  }
}

void BoolColumn::cast_into(PyObjectColumn* target) const {
  int8_t*    src_data = this->elements();
  PyObject** trg_data = target->elements();
  for (int64_t i = 0; i < nrows; ++i) {
    int8_t x = src_data[i];
    trg_data[i] = x == 1? Py_True : x == 0? Py_False : Py_None;
    Py_INCREF(trg_data[i]);
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
