//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <Python.h>
#include "column.h"
#include "datatablemodule.h"
#include "utils/parallel.h"




SType BoolColumn::stype() const noexcept {
  return SType::BOOL;
}

py::oobj BoolColumn::get_value_at_index(size_t i) const {
  size_t j = ri[i];
  int8_t x = elements_r()[j];
  return ISNA(x)? py::None() : x? py::True() : py::False();
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
typedef std::unique_ptr<MemoryWritableBuffer> MWBPtr;

template <typename T>
inline static void cast_helper(size_t nrows, const int8_t* src, T* trg) {
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < nrows; ++i) {
    int8_t x = src[i];
    trg[i] = ISNA<int8_t>(x)? GETNA<T>() : static_cast<T>(x);
  }
}

template <typename T>
inline static MemoryRange cast_str_helper(
  const BoolColumn* src, StringColumn<T>* target)
{
  const int8_t* src_data = src->elements_r();
  T* toffsets = target->offsets_w();
  size_t exp_size = src->nrows;
  auto wb = MWBPtr(new MemoryWritableBuffer(exp_size));
  char* tmpbuf = new char[1024];
  TRACK(tmpbuf, sizeof(tmpbuf), "BoolColumn::tmpbuf");
  char* tmpend = tmpbuf + 1000;  // Leave at least 24 spare chars in buffer
  char* ch = tmpbuf;
  T offset = 0;
  toffsets[-1] = 0;
  for (size_t i = 0; i < src->nrows; ++i) {
    int8_t x = src_data[i];
    if (ISNA<int8_t>(x)) {
      toffsets[i] = offset | GETNA<T>();
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
  UNTRACK(tmpbuf);
  return wb->get_mbuf();
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
  for (size_t i = 0; i < nrows; ++i) {
    trg_data[i] = bool_to_py(src_data[i]);
  }
}

void BoolColumn::cast_into(StringColumn<uint32_t>* target) const {
  MemoryRange strbuf = cast_str_helper<uint32_t>(this, target);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

void BoolColumn::cast_into(StringColumn<uint64_t>* target) const {
  MemoryRange strbuf = cast_str_helper<uint64_t>(this, target);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}



//------------------------------------------------------------------------------
// Integrity checks
//------------------------------------------------------------------------------

void BoolColumn::verify_integrity(const std::string& name) const {
  FwColumn<int8_t>::verify_integrity(name);

  // Check that all elements in column are either 0, 1, or NA_I1
  size_t mbuf_nrows = data_nrows();
  const int8_t* vals = elements_r();
  for (size_t i = 0; i < mbuf_nrows; ++i) {
    int8_t val = vals[i];
    if (!(val == 0 || val == 1 || val == NA_I1)) {
      throw AssertionError()
          << "(Boolean) " << name << " has value " << val << " in row " << i;
    }
  }
}
