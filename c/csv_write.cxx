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
#include <new>          // placement new
#include <stdexcept>    // std::runtime_error
#include <errno.h>      // errno
#include <fcntl.h>      // open
#include <stdint.h>     // int32_t, etc
#include <stdio.h>      // printf
#include <string.h>     // strerror
#include <sys/mman.h>   // mmap
#include "column.h"
#include "csv.h"
#include "csv_lookups.h"
#include "datatable.h"
#include "math.h"
#include "memorybuf.h"
#include "myomp.h"
#include "types.h"
#include "utils.h"


class CsvColumn;
inline static void write_int32(char **pch, int32_t value);
static void write_string(char **pch, const char *value);

typedef void (*writer_fn)(char **pch, CsvColumn* col, int64_t row);

static size_t bytes_per_stype[DT_STYPES_COUNT];
static writer_fn writers_per_stype[DT_STYPES_COUNT];

// Helper lookup table for writing integers
static const int32_t DIVS32[10] = {
  1,
  10,
  100,
  1000,
  10000,
  100000,
  1000000,
  10000000,
  100000000,
  1000000000,
};

static const int64_t DIVS64[19] = {
  1L,
  10L,
  100L,
  1000L,
  10000L,
  100000L,
  1000000L,
  10000000L,
  100000000L,
  1000000000L,
  10000000000L,
  100000000000L,
  1000000000000L,
  10000000000000L,
  100000000000000L,
  1000000000000000L,
  10000000000000000L,
  100000000000000000L,
  1000000000000000000L,
};

#define F64_SIGN_MASK  0x8000000000000000u
#define F64_MANT_MASK  0x000FFFFFFFFFFFFFu
#define F64_EXTRA_BIT  0x0010000000000000u
#define F64_1em5       0x3EE4F8B588E368F1u
#define F64_1e15       0x430c6bf526340000u
#define TENp18         1000000000000000000


// TODO: replace with classes that derive from CsvColumn and implement write()
class CsvColumn {
public:
  void *data;
  char *strbuf;
  writer_fn writer;

  CsvColumn(Column *col) {
    data = col->data;
    strbuf = NULL;
    writer = writers_per_stype[col->stype];
    if (!writer) throw std::runtime_error("Cannot write this type");
    if (col->stype == ST_STRING_I4_VCHAR) {
      strbuf = reinterpret_cast<char*>(data) - 1;
      data = strbuf + 1 + ((VarcharMeta*)col->meta)->offoff;
    }
  }

  void write(char **pch, int64_t row) {
    writer(pch, this, row);
  }
};

#define VLOG(...)  do { if (verbose) log_message(logger, __VA_ARGS__); } while (0)


//=================================================================================================
// Field writers
//
// Note: we attempt to optimize these functions for speed. See /microbench/writecsv for various
// experiments and benchmarks.
//=================================================================================================

static void write_b1(char **pch, CsvColumn *col, int64_t row)
{
  int8_t value = reinterpret_cast<int8_t*>(col->data)[row];
  **pch = static_cast<char>(value) + '0';
  *pch += (value != NA_I1);
}


static void write_i1(char **pch, CsvColumn *col, int64_t row)
{
  int value = static_cast<int>(reinterpret_cast<int8_t*>(col->data)[row]);
  char *ch = *pch;
  if (value == NA_I1) return;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  if (value >= 100) {  // the range of `value` is up to 127
    *ch++ = '1';
    int d = value/10;
    *ch++ = static_cast<char>(d) - 10 + '0';
    value -= d*10;
  } else if (value >= 10) {
    int d = value/10;
    *ch++ = static_cast<char>(d) + '0';
    value -= d*10;
  }
  *ch++ = static_cast<char>(value) + '0';
  *pch = ch;
}


static void write_i2(char **pch, CsvColumn *col, int64_t row)
{
  int value = ((int16_t*) col->data)[row];
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  if (value == NA_I2) return;
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 1000)? 2 : 4;
  for (; value < DIVS32[r]; r--);
  for (; r; r--) {
    int d = value / DIVS32[r];
    *ch++ = static_cast<char>(d) + '0';
    value -= d * DIVS32[r];
  }
  *ch = static_cast<char>(value) + '0';
  *pch = ch + 1;
}

static inline void write_int32(char **pch, int32_t value) {
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 100000)? 4 : 9;
  for (; value < DIVS32[r]; r--);
  for (; r; r--) {
    int d = value / DIVS32[r];
    *ch++ = static_cast<char>(d) + '0';
    value -= d * DIVS32[r];
  }
  *ch = static_cast<char>(value) + '0';
  *pch = ch + 1;
}

static inline void write_int64(char **pch, int64_t value) {
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 10000000)? 6 : 18;
  for (; value < DIVS64[r]; r--);
  for (; r; r--) {
    int64_t d = value / DIVS64[r];
    *ch++ = static_cast<char>(d) + '0';
    value -= d * DIVS64[r];
  }
  *ch = static_cast<char>(value) + '0';
  *pch = ch + 1;
}


static void write_i4(char **pch, CsvColumn *col, int64_t row)
{
  int32_t value = ((int32_t*) col->data)[row];
  if (value == NA_I4) return;
  write_int32(pch, value);
}


static void write_i8(char **pch, CsvColumn *col, int64_t row)
{
  int64_t value = ((int64_t*) col->data)[row];
  if (value == NA_I8) return;
  write_int64(pch, value);
}


static void write_s4(char **pch, CsvColumn *col, int64_t row)
{
  int32_t offset1 = ((int32_t*) col->data)[row];
  int32_t offset0 = abs(((int32_t*) col->data)[row - 1]);
  char *ch = *pch;

  if (offset1 < 0) return;
  if (offset0 == offset1) {
    ch[0] = '"';
    ch[1] = '"';
    *pch = ch + 2;
    return;
  }
  const uint8_t *strstart = reinterpret_cast<uint8_t*>(col->strbuf) + offset0;
  const uint8_t *strend = reinterpret_cast<uint8_t*>(col->strbuf) + offset1;
  const uint8_t *sch = strstart;
  if (*sch == 32) goto quote;
  while (sch < strend) {  // ',' is 44, '"' is 34
    uint8_t c = *sch;
    // First `c <= 44` is to give an opportunity to short-circuit early.
    if (c <= 44 && (c == 44 || c == 34 || c < 32)) break;
    *ch++ = static_cast<char>(c);
    sch++;
  }
  if (sch < strend || sch[-1] == 32) {
    quote:
    ch = *pch;
    memcpy(ch+1, strstart, static_cast<size_t>(sch - strstart));
    *ch = '"';
    ch += sch - strstart + 1;
    while (sch < strend) {
      if (*sch == '"') *ch++ = '"';  // double the quote
      *ch++ = *sch++;
    }
    *ch++ = '"';
  }
  *pch = ch;
}


static char hexdigits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
static void write_f8_hex(char **pch, CsvColumn *col, int64_t row)
{
  // Read the value as if it was uint64_t
  uint64_t value = ((uint64_t*) col->data)[row];
  char *ch = *pch;

  int exp = (int)(value >> 52);
  uint64_t sig = (value & 0xFFFFFFFFFFFFF);
  if (exp & 0x800) {
    *ch++ = '-';
    exp ^= 0x800;
  }
  if (exp == 0x7FF) {  // nan & inf
    if (sig == 0) {  // - sign was already printed, if any
      ch[0] = 'i'; ch[1] = 'n'; ch[2] = 'f';
    } else {
      ch[0] = 'n'; ch[1] = 'a'; ch[2] = 'n';
    }
    *pch = ch + 3;
    return;
  }
  ch[0] = '0';
  ch[1] = 'x';
  ch[2] = '0' + (exp != 0x000);
  ch[3] = '.';
  ch += 3 + (sig != 0);
  while (sig) {
    uint64_t r = sig & 0xF000000000000;
    *ch++ = hexdigits[r >> 48];
    sig = (sig ^ r) << 4;
  }
  if (exp) exp -= 0x3FF;
  *ch++ = 'p';
  *ch++ = '+' + (exp < 0)*('-' - '+');
  write_int32(&ch, abs(exp));
  *pch = ch;
}


static void write_f4_hex(char **pch, CsvColumn *col, int64_t row)
{
  // Read the value as if it was uint32_t
  uint32_t value = static_cast<uint32_t*>(col->data)[row];
  char *ch = *pch;

  int exp = static_cast<int>(value >> 23);
  uint32_t sig = (value & 0x7FFFFF);
  if (exp & 0x100) {
    *ch++ = '-';
    exp ^= 0x100;
  }
  if (exp == 0xFF) {  // nan & inf
    if (sig == 0) {  // - sign was already printed, if any
      ch[0] = 'i'; ch[1] = 'n'; ch[2] = 'f';
    } else {
      ch[0] = 'n'; ch[1] = 'a'; ch[2] = 'n';
    }
    *pch = ch + 3;
    return;
  }
  ch[0] = '0';
  ch[1] = 'x';
  ch[2] = '0' + (exp != 0x00);
  ch[3] = '.';
  ch += 3 + (sig != 0);
  while (sig) {
    uint32_t r = sig & 0x780000;
    *ch++ = hexdigits[r >> 19];
    sig = (sig ^ r) << 4;
  }
  if (exp) exp -= 0x7F;
  *ch++ = 'p';
  *ch++ = '+' + (exp < 0)*('-' - '+');
  write_int32(&ch, abs(exp));
  *pch = ch;
}

// Helper for write_f8_dec
static inline void write_exponent(char **pch, int value) {
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  } else {
    *ch++ = '+';
  }
  if (value >= 100) {
    int d = value/100;
    *ch++ = static_cast<char>(d) + '0';
    value -= d*100;
    d = value/10;
    *ch++ = static_cast<char>(d) + '0';
    value -= d*10;
  } else if (value >= 10) {
    int d = value/10;
    *ch++ = static_cast<char>(d) + '0';
    value -= d*10;
  }
  *ch++ = static_cast<char>(value) + '0';
  *pch = ch;
}


/**
 * The problem of converting a floating-point number (float64) into a string
 * can be formulated as follows (assume x is positive and normal):
 *
 *   1. First, the "input" value v is decomposed into the mantissa and the
 *      exponent parts:
 *
 *          x = f * 2^e = F * 2^(e - 52)
 *
 *      where F is uint64, and e is int. These parts can be computed using
 *      simple bit operations on `v = reinterpret_cast<uint64>(x)`:
 *
 *          F = (v & (1<<52 - 1)) | (1<<52)
 *          e = ((v >> 52) & 0x7FF) - 0x3FF
 *
 *   2. We'd like to find integer numbers D and E such that
 *
 *          x ≈ d * 10^E = D * 10^(E - 17)
 *
 *      where 10^17 <= D < 10^18. If such numbers are found, then producing
 *      the final string is simple, one of the following forms can be used:
 *
 *          D[0] '.' D[1:] 'e' E
 *          D[0:E] '.' D[E:]
 *          '0.' '0'{-E-1} D
 *
 *   3. Denote f = F*2^-52, and d = D*10^-17. Then 1 <= f < 2, and similarly
 *      1 <= d < 10. Therefore,
 *
 *          E = log₁₀(f) + e * log₁₀2 - log₁₀(d)
 *          E = Floor[log₁₀(f) + e * log₁₀2]
 *          E ≤ Floor[1 + e * log₁₀2]
 *
 *      This may overestimate E by 1, but ultimately it doesn't matter... In
 *      practice we can replace this formula with one that is close numerically
 *      but easier to compute:
 *
 *          E = Floor[(e*1233 - 8)/4096] = ((201 + eb*1233)>>12) - 308
 *
 *      where eb = e + 0x3FF is the biased exponent.
 *
 *   4. Then, D can be computed as
 *
 *          D = Floor[F * 2^(e - 52) * 10^(17 - E(e))]
 *
 *      (with the choice of E(e) as above, this quantity will range from
 *      1.001e17 to 2.015e18. The coefficients in E(e) were optimized in order
 *      to minimize sup(D) subject to inf(D)>=1e17.)
 *
 *      In this expression, F is integer, whereas 2^(e-52) * 10^(17-E(e)) is
 *      float. If we write the latter as (A+B)/2^53 (where B < 1), then
 *      expression for D becomes
 *
 *          D = Floor[(F * A)/2^53 + (F * B)/2^53]
 *
 *      Here F<2^53 and B<1, and therefore the second term is negligible.
 *      Thus,
 *
 *         D = (F * A(e)) >> 53
 *         A(e) = Floor[2^(e+1) * 10^(17-E(e))]
 *
 *      The quantities A(e) are `uint64`s in the range approximately from
 *      2.002e17 to 2.015e18. They can be precomputed and stored for every
 *      exponent e (there are 2046 of them). Multiplying and shifting of
 *      two uint64 numbers is fast and simple.
 *
 * This algorithm is roughly similar to Grisu2.
 */
static inline void write_double(char **pch, double value)
{
  char *ch = *pch;
  union { double d; uint64_t u; } vvv = { value };
  uint64_t value_u64 = vvv.u;

  if (value_u64 & F64_SIGN_MASK) {
    *ch++ = '-';
    value_u64 ^= F64_SIGN_MASK;
  }
  if (value_u64 > F64_1em5 && value_u64 < F64_1e15) {
    union { uint64_t u; double d; } vvv = { value_u64 };
    double frac, intval;
    frac = modf(vvv.d, &intval);
    char *ch0 = ch;
    write_int64(&ch, static_cast<int64_t>(intval));

    if (frac > 0) {
      int digits_left = 15 - static_cast<int>(ch - ch0);
      *ch++ = '.';
      while (frac > 0 && digits_left) {
        frac *= 10;
        frac = modf(frac, &intval);
        *ch++ = static_cast<char>(intval) + '0';
        digits_left--;
      }
      if (!digits_left) {
        intval = frac*10 + 0.5;
        if (intval > 9) intval = 9;
        *ch++ = static_cast<char>(intval) + '0';
      }
    }
    *pch = ch;
    return;
  }

  int eb = static_cast<int>(value_u64 >> 52);
  if (eb == 0x7FF) {
    if (!value_u64) {  // don't print nans at all
      *ch++ = 'i'; *ch++ = 'n'; *ch++ = 'f';
      *pch = ch;
    }
    return;
  } else if (eb == 0x000) {
    *ch++ = '0';
    *pch = ch;
    return;
  }
  int E = ((201 + eb*1233) >> 12) - 308;
  uint64_t F = (value_u64 & F64_MANT_MASK) | F64_EXTRA_BIT;
  uint64_t A = Atable[eb];
  unsigned __int128 p = static_cast<unsigned __int128>(F) *
                        static_cast<unsigned __int128>(A);
  int64_t D = static_cast<int64_t>(p >> 53) + (static_cast<int64_t>(p) >> 53);
  if (D >= TENp18) {
    D /= 10;
    E++;
  }
  ch += 18;
  char *tch = ch;
  for (int r = 18; r; r--) {
    ldiv_t ddd = ldiv(D, 10);
    D = ddd.quot;
    *tch-- = static_cast<char>(ddd.rem) + '0';
    if (r == 2) { *tch-- = '.'; }
  }
  while (ch[-1] == '0') ch--;  // remove any trailing 0s
  *ch++ = 'e';
  write_exponent(&ch, E);
  *pch = ch;
}


static void write_f8_dec(char **pch, CsvColumn *col, int64_t row) {
  double value = ((double*) col->data)[row];
  if (isnan(value)) return;
  write_double(pch, value);
}

static void write_f4_dec(char **pch, CsvColumn *col, int64_t row) {
  float value = ((float*) col->data)[row];
  if (isnan(value)) return;
  write_double(pch, static_cast<double>(value));
}


/**
 * This function writes a plain 0-terminated C string. This is not a regular
 * "writer" -- instead it may be used to write extra data to the file, such
 * as headers/footers.
 */
static void write_string(char **pch, const char *value)
{
  char *ch = *pch;
  const char *sch = value;
  // If the field begins with a space, it has to be quoted.
  if (*value == ' ') goto quote;
  for (;;) {
    char c = *sch++;
    if (!c) {
      // If the field is empty, or ends with whitespace, then it has to be
      // quoted as well.
      if (sch == value + 1 || sch[-2] == ' ') break;
      *pch = ch;
      return;
    }
    if (c == '"' || c == ',' || static_cast<uint8_t>(c) < 32) break;
    *ch++ = c;
  }
  // If we broke out of the loop above, it means we need to quote the field.
  // So, first rewind to the beginning
  quote:
  ch = *pch;
  sch = value;
  *ch++ = '"';
  for (;;) {
    char c = *sch++;
    if (!c) break;
    if (c == '"') *ch++ = '"';  // double the quote
    *ch++ = c;
  }
  *ch++ = '"';
  *pch = ch;
}



//=================================================================================================
//
// Main CSV-writing functions
//
//=================================================================================================

CsvWriter::CsvWriter(DataTable *dt_, const std::string& path_)
  : dt(dt_),
    path(path_),
    logger(nullptr),
    nthreads(1),
    usehex(false),
    verbose(false),
    wb(nullptr),
    t_last(0)
{}


CsvWriter::~CsvWriter()
{
  delete wb;
  for (size_t i = 0; i < columns.size(); i++)
    delete columns[i];
}


void CsvWriter::write()
{
  int64_t nrows = dt->nrows;
  size_t ncols = static_cast<size_t>(dt->ncols);
  size_t bytes_total = estimate_output_size();
  create_target(bytes_total);
  write_column_names();
  determine_chunking_strategy(bytes_total, nrows);
  create_column_writers(ncols);

  OmpExceptionManager oem;
  #define OMPCODE(code)  try { code } catch (...) { oem.capture_exception(); }

  // Start writing the CSV
  #pragma omp parallel num_threads(nthreads)
  {
    #pragma omp single
    {
      VLOG("Writing file using %lld chunks, with %.1f rows per chunk\n",
           nchunks, rows_per_chunk);
      VLOG("Using nthreads = %d\n", omp_get_num_threads());
      VLOG("Initial buffer size in each thread: %zu\n", bytes_per_chunk*2);
    }
    // Initialize thread-local variables
    size_t thbufsize = bytes_per_chunk * 2;
    char  *thbuf = nullptr;
    size_t th_write_at = 0;
    size_t th_write_size = 0;
    OMPCODE(
      thbuf = new char[thbufsize];
    )

    // Main data-writing loop
    #pragma omp for ordered schedule(dynamic)
    for (int64_t i = 0; i < nchunks; i++) {
      if (oem.exception_caught()) continue;
      int64_t row0 = static_cast<int64_t>(i * rows_per_chunk);
      int64_t row1 = static_cast<int64_t>((i + 1) * rows_per_chunk);
      if (i == nchunks-1) row1 = nrows;  // always go to the last row for last chunk

      OMPCODE(
        // write the thread-local buffer into the output
        if (th_write_size) {
          wb->write_at(th_write_at, th_write_size, thbuf);
        }

        char *thch = thbuf;
        for (int64_t row = row0; row < row1; row++) {
          for (size_t col = 0; col < ncols; col++) {
            columns[col]->write(&thch, row);
            *thch++ = ',';
          }
          thch[-1] = '\n';
        }
        th_write_size = static_cast<size_t>(thch - thbuf);
      )

      #pragma omp ordered
      {
        OMPCODE(
          th_write_at = wb->prep_write(th_write_size, thbuf);
        )
      }
    }
    OMPCODE(
      if (th_write_size && !oem.exception_caught()) {
        wb->write_at(th_write_at, th_write_size, thbuf);
      }
      delete[] thbuf;
    )
  }
  oem.rethrow_exception_if_any();
  t_write_data = checkpoint();

  // Done writing; if writing to stdout then append '\0' to make it a regular
  // C string; otherwise truncate MemoryBuffer to the final size.
  VLOG("Finalizing output at size %s\n", filesize_to_str(wb->size()));
  if (path.empty()) {
    char c = '\0';
    wb->write(1, &c);
  }
  wb->finalize();
  t_finalize = checkpoint();

  double t_total = t_prepare_for_writing + t_size_estimation + t_create_target
                   + t_write_data + t_finalize;
  VLOG("Timing report:\n");
  VLOG("   %6.3fs  Calculate expected file size\n", t_size_estimation);
  VLOG(" + %6.3fs  Allocate file\n",                t_create_target);
  VLOG(" + %6.3fs  Prepare for writing\n",          t_prepare_for_writing);
  VLOG(" + %6.3fs  Write the data\n",               t_write_data);
  VLOG(" + %6.3fs  Finalize the file\n",            t_finalize);
  VLOG(" = %6.3fs  Overall time taken\n",           t_total);
}


/**
 * Convenience function to measure duration of certain steps. When called
 * returns the time elapsed from the previous call to this function.
 */
double CsvWriter::checkpoint() {
  double t_previous = t_last;
  t_last = wallclock();
  return t_last - t_previous;
}


/**
 * Estimate and return the expected size of the output.
 *
 * The size of string columns is estimated liberally, assuming it may
 * get inflated by no more than 20% (+2 chars for the quotes). If the data
 * contains many quotes, they may inflate more than this.
 * The size of numeric columns is estimated conservatively: we compute the
 * maximum amount of space that is theoretically required.
 * Overall, we will probably overestimate the final size of the CSV by a big
 * margin.
 */
size_t CsvWriter::estimate_output_size()
{
  size_t nrows = static_cast<size_t>(dt->nrows);
  size_t ncols = static_cast<size_t>(dt->ncols);
  size_t bytes_total = 0;
  for (size_t i = 0; i < ncols; i++) {
    Column *col = dt->columns[i];
    SType stype = col->stype;
    if (stype == ST_STRING_I4_VCHAR || stype == ST_STRING_I8_VCHAR) {
      size_t datasize = stype == ST_STRING_I4_VCHAR? column_i4s_datasize(col)
                                                   : column_i8s_datasize(col);
      bytes_total += static_cast<size_t>(1.2 * datasize + 2 * nrows);
    } else {
      bytes_total += bytes_per_stype[stype] * nrows;
    }
  }
  bytes_total += ncols * nrows;  // Account for separators / newlines
  VLOG("Estimated output size: %zu\n", bytes_total);
  t_size_estimation = checkpoint();
  return bytes_total;
}


/**
 * Create the target memory region (either in RAM, or on disk).
 */
void CsvWriter::create_target(size_t size)
{
  if (path.empty()) {
    wb = new MemoryWritableBuffer(size);
  } else {
    // TODO: on MacOS, create FileWritableBuffer instead
    VLOG("Creating destination file of size %s\n", filesize_to_str(size));
    wb = new MmapWritableBuffer(path, size);
  }
  t_create_target = checkpoint();
}


/**
 * Write the first row of column names into the output.
 */
void CsvWriter::write_column_names()
{
  size_t ncolnames = column_names.size();
  if (ncolnames) {
    size_t maxsize = 0;
    for (size_t i = 0; i < ncolnames; i++) {
      // A string may expand up to twice its original size (if all characters
      // need to be escaped) + add 2 surrounding quotes + add a comma.
      maxsize += column_names[i].size()*2 + 2 + 1;
    }
    char *ch0 = new char[maxsize];
    char *ch = ch0;
    for (size_t i = 0; i < ncolnames; i++) {
      write_string(&ch, column_names[i].data());
      *ch++ = ',';
    }
    // Replace the last ',' with a newline. This is valid since `ncolnames > 0`.
    ch[-1] = '\n';

    // Write this string buffer into the target.
    wb->write(static_cast<size_t>(ch - ch0), ch0);
    delete[] ch0;
  }
}


/**
 * Compute parameters for writing the file: how many chunks to use, how many
 * rows per chunk, etc.
 *
 * This function depends only on parameters `bytes_total`, `nrows` and
 * `nthreads`. Its effect is to fill in values `rows_per_chunk`, 'nchunks' and
 * `bytes_per_chunk`.
 */
void CsvWriter::determine_chunking_strategy(size_t bytes_total, int64_t nrows)
{
  static const size_t max_chunk_size = 1024 * 1024;
  static const size_t min_chunk_size = 1024;

  double bytes_per_row = nrows? 1.0 * bytes_total / nrows : 0;
  int min_nchunks = nthreads == 1 ? 1 : nthreads*2;
  nchunks = 1 + (bytes_total - 1) / max_chunk_size;
  if (nchunks < min_nchunks) nchunks = min_nchunks;
  int attempts = 5;
  while (attempts--) {
    rows_per_chunk = 1.0 * (nrows + 1) / nchunks;
    bytes_per_chunk = static_cast<size_t>(bytes_per_row * rows_per_chunk);
    if (rows_per_chunk < 1.0) {
      // If each row's size is too large, then parse 1 row at a time.
      nchunks = nrows;
    } else if (bytes_per_chunk < min_chunk_size && nchunks > 1) {
      // The data is too small, and number of available threads too large --
      // reduce the number of chunks so that we don't waste resources on
      // needless thread manipulation.
      // This formula guarantees that new bytes_per_chunk will be no less
      // than min_chunk_size (or nchunks will be 1).
      nchunks = bytes_total / min_chunk_size;
      if (nchunks < 1) nchunks = 1;
    } else {
      return;
    }
  }
  // This shouldn't really happen, but who knows...
  throw Error("Unable to determine how to write the file: bytes_total=%zu, "
              "nrows=%zd, nthreads=%d, min.chunk=%zu, max.chunk=%zu",
              bytes_total, nrows, nthreads, min_chunk_size, max_chunk_size);
}


/**
 */
void CsvWriter::create_column_writers(size_t ncols)
{
  columns.reserve(ncols);
  writers_per_stype[ST_REAL_F4] = usehex? write_f4_hex : write_f4_dec;
  writers_per_stype[ST_REAL_F8] = usehex? write_f8_hex : write_f8_dec;
  for (int64_t i = 0; i < dt->ncols; i++) {
    Column *dtcol = dt->columns[i];
    SType stype = dtcol->stype;
    CsvColumn *csvcol = new CsvColumn(dtcol);
    columns.push_back(csvcol);
    if (stype == ST_STRING_I4_VCHAR || stype == ST_STRING_I8_VCHAR) {
      strcolumns.push_back(csvcol);
    }
  }
  t_prepare_for_writing = checkpoint();
}



//=================================================================================================
// Helper functions
//=================================================================================================

void init_csvwrite_constants() {

  for (int i = 0; i < DT_STYPES_COUNT; i++) {
    bytes_per_stype[i] = 0;
    writers_per_stype[i] = NULL;
  }
  bytes_per_stype[ST_BOOLEAN_I1] = 1;  // 1
  bytes_per_stype[ST_INTEGER_I1] = 4;  // -100
  bytes_per_stype[ST_INTEGER_I2] = 6;  // -32000
  bytes_per_stype[ST_INTEGER_I4] = 11; // -2000000000
  bytes_per_stype[ST_INTEGER_I8] = 20; // -9223372036854775800
  bytes_per_stype[ST_REAL_F4]    = 25; // -0x1.123456p+30
  bytes_per_stype[ST_REAL_F8]    = 25; // -0x1.23456789ABCDEp+1000

  writers_per_stype[ST_BOOLEAN_I1] = (writer_fn) write_b1;
  writers_per_stype[ST_INTEGER_I1] = (writer_fn) write_i1;
  writers_per_stype[ST_INTEGER_I2] = (writer_fn) write_i2;
  writers_per_stype[ST_INTEGER_I4] = (writer_fn) write_i4;
  writers_per_stype[ST_INTEGER_I8] = (writer_fn) write_i8;
  writers_per_stype[ST_REAL_F4]    = (writer_fn) write_f4_dec;
  writers_per_stype[ST_REAL_F8]    = (writer_fn) write_f8_dec;
  writers_per_stype[ST_STRING_I4_VCHAR] = (writer_fn) write_s4;
}
