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
#include "myomp.h"



BoolColumn::~BoolColumn() {}


SType BoolColumn::stype() const {
  return ST_BOOLEAN_I1;
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

/**
 * See DataTable::verify_integrity for method description
 */
int BoolColumn::verify_integrity(
    std::vector<char> *errors, int max_errors, const char *name) const
{
  // Check general properties
  int nerrors = FwColumn<int8_t>::verify_integrity(errors, max_errors, name);
  if (nerrors > 0) return nerrors;
  if (errors == nullptr) return nerrors; // TODO: do something different?

  // Check that all elements in column are either 0, 1, or NA_I1
  int64_t mbuf_nrows = data_nrows();
  int8_t *vals = elements();
  for (int64_t i = 0; i < mbuf_nrows; ++i) {
    int8_t val = vals[i];
    if (!(val == 0 || val == 1 || val == NA_I1)) {
      ERR("%s is of Boolean type but has value %d in row %lld\n", name, val, i);
    }
  }
  return nerrors;
}
