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

int8_t  BoolColumn::min() const  { return get_stats()->min(this); }
int8_t  BoolColumn::max() const  { return get_stats()->max(this); }
int8_t  BoolColumn::mode() const { return get_stats()->mode(this); }
int64_t BoolColumn::sum() const  { return get_stats()->sum(this); }
double  BoolColumn::mean() const { return get_stats()->mean(this); }
double  BoolColumn::sd() const   { return get_stats()->stdev(this); }

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

Column* BoolColumn::mode_column() const {
  BoolColumn* col = new BoolColumn(1);
  col->set_elem(0, mode());
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
PyObject* BoolColumn::mode_pyscalar() const { return bool_to_py(mode()); }
PyObject* BoolColumn::sum_pyscalar() const { return int_to_py(sum()); }
PyObject* BoolColumn::mean_pyscalar() const { return float_to_py(mean()); }
PyObject* BoolColumn::sd_pyscalar() const { return float_to_py(sd()); }



//------------------------------------------------------------------------------
// Type casts
//------------------------------------------------------------------------------

template <typename T>
inline static void cast_helper(int64_t nrows, const int8_t* src, T* trg) {
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < nrows; ++i) {
    int8_t x = src[i];
    trg[i] = ISNA<int8_t>(x)? GETNA<T>() : static_cast<T>(x);
  }
}

template <typename T>
inline static MemoryBuffer* cast_str_helper(
  int64_t nrows, const int8_t* src, T* toffsets)
{
  size_t exp_size = static_cast<size_t>(nrows);
  MemoryWritableBuffer* wb = new MemoryWritableBuffer(exp_size);
  char* tmpbuf = new char[1024];
  char* tmpend = tmpbuf + 1000;  // Leave at least 24 spare chars in buffer
  char* ch = tmpbuf;
  T offset = 1;
  toffsets[-1] = -1;
  for (int64_t i = 0; i < nrows; ++i) {
    int8_t x = src[i];
    if (ISNA<int8_t>(x)) {
      toffsets[i] = -offset;
    } else {
      *ch++ = '0' + x;
      offset++;
      toffsets[i] = offset;
      if (ch > tmpend) {
        wb->write(static_cast<size_t>(ch - tmpbuf), tmpbuf);
        ch = tmpbuf;
      }
    }
  }
  wb->write(static_cast<size_t>(ch - tmpbuf), tmpbuf);
  wb->finalize();
  delete[] tmpbuf;
  MemoryBuffer* res = wb->get_mbuf();
  delete wb;
  return res;
}


void BoolColumn::cast_into(BoolColumn* target) const {
  std::memcpy(target->data_w(), data(), alloc_size());
}

void BoolColumn::cast_into(IntColumn<int8_t>* target) const {
  std::memcpy(target->data_w(), data(), alloc_size());
}

void BoolColumn::cast_into(IntColumn<int16_t>* target) const {
  cast_helper<int16_t>(nrows, this->elements_r(), target->elements_w());
}

void BoolColumn::cast_into(IntColumn<int32_t>* target) const {
  cast_helper<int32_t>(nrows, this->elements_r(), target->elements_w());
}

void BoolColumn::cast_into(IntColumn<int64_t>* target) const {
  cast_helper<int64_t>(nrows, this->elements_r(), target->elements_w());
}

void BoolColumn::cast_into(RealColumn<float>* target) const {
  cast_helper<float>(nrows, this->elements_r(), target->elements_w());
}

void BoolColumn::cast_into(RealColumn<double>* target) const {
  cast_helper<double>(nrows, this->elements_r(), target->elements_w());
}

void BoolColumn::cast_into(PyObjectColumn* target) const {
  const int8_t* src_data = this->elements_r();
  PyObject**    trg_data = target->elements_w();
  for (int64_t i = 0; i < nrows; ++i) {
    trg_data[i] = bool_to_py(src_data[i]);
  }
}

void BoolColumn::cast_into(StringColumn<int32_t>* target) const {
  MemoryRange data = target->data_buf();
  int32_t* offsets = static_cast<int32_t*>(data.wptr()) + 1;
  MemoryBuffer* strbuf = cast_str_helper<int32_t>(nrows, elements_r(), offsets);
  target->replace_buffer(std::move(data), strbuf);
}

void BoolColumn::cast_into(StringColumn<int64_t>* target) const {
  MemoryRange data = target->data_buf();
  int64_t* offsets = static_cast<int64_t*>(data.wptr()) + 1;
  MemoryBuffer* strbuf = cast_str_helper<int64_t>(nrows, elements_r(), offsets);
  target->replace_buffer(std::move(data), strbuf);
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
  const int8_t* vals = elements_r();
  for (int64_t i = 0; i < mbuf_nrows; ++i) {
    int8_t val = vals[i];
    if (!(val == 0 || val == 1 || val == NA_I1)) {
      icc << "(Boolean) " << name << " has value " << val << " in row " << i
          << end;
    }
  }
  return !icc.has_errors(nerrors);
}
