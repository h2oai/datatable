//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <new>          // placement new
#include <stdexcept>    // std::runtime_error
#include <math.h>
#include <stdint.h>     // int32_t, etc
#include <stdio.h>      // printf
#include "csv/toa.h"
#include "csv/writer.h"
#include "utils/alloc.h"
#include "utils/parallel.h"
#include "column.h"
#include "datatable.h"
#include "datatablemodule.h"
#include "memrange.h"
#include "types.h"
#include "utils.h"


class CsvColumn;
static void write_string(char** pch, const char* value);

typedef void (*writer_fn)(char** pch, CsvColumn* col, size_t row);

static size_t bytes_per_stype[DT_STYPES_COUNT];
static writer_fn writers_per_stype[DT_STYPES_COUNT];



// TODO: replace with classes that derive from CsvColumn and implement write()
class CsvColumn {
public:
  const void* data;
  const char* strbuf;
  writer_fn writer;

  explicit CsvColumn(Column* col) {
    data = col->data();
    strbuf = nullptr;
    writer = writers_per_stype[static_cast<int>(col->stype())];
    if (!writer) {
      throw ValueError() << "Cannot write type " << col->stype();
    }
    if (col->stype() == SType::STR32) {
      strbuf = static_cast<StringColumn<uint32_t>*>(col)->strdata();
      data = static_cast<StringColumn<uint32_t>*>(col)->offsets();
    }
    else if (col->stype() == SType::STR64) {
      strbuf = static_cast<StringColumn<uint64_t>*>(col)->strdata();
      data = static_cast<StringColumn<uint64_t>*>(col)->offsets();
    }
    TRACK(this, sizeof(*this), "write::CsvColumn");
  }

  ~CsvColumn() {
    UNTRACK(this);
  }

  void write(char** pch, size_t row) {
    writer(pch, this, row);
  }

  // This should only be called on a CsvColumn of type i4s / i8s!
  template <typename T>
  size_t strsize(size_t row0, size_t row1) {
    const T* offsets = static_cast<const T*>(data);
    return (offsets[row1 - 1] - offsets[row0 - 1]) & ~GETNA<T>();
  }
};

#define VLOG(...)  do { if (verbose) log_message(logger, __VA_ARGS__); } while (0)


//=================================================================================================
// Field writers
//
// Note: we attempt to optimize these functions for speed. See /microbench/writecsv for various
// experiments and benchmarks.
//=================================================================================================

static void write_b1(char** pch, CsvColumn* col, size_t row) {
  int8_t value = static_cast<const int8_t*>(col->data)[row];
  **pch = static_cast<char>(value) + '0';
  *pch += (value != NA_I1);
}


template <typename T>
void write_iN(char** pch, CsvColumn* col, size_t row) {
  T value = static_cast<const T*>(col->data)[row];
  if (ISNA<T>(value)) return;
  toa<T>(pch, value);
}


template <typename T>
void write_str(char** pch, CsvColumn* col, size_t row)
{
  T offset1 = (static_cast<const T*>(col->data))[row];
  T offset0 = (static_cast<const T*>(col->data))[row - 1] & ~GETNA<T>();
  char *ch = *pch;

  if (ISNA<T>(offset1)) return;
  if (offset0 == offset1) {
    ch[0] = '"';
    ch[1] = '"';
    *pch = ch + 2;
    return;
  }
  const uint8_t* strstart = reinterpret_cast<const uint8_t*>(col->strbuf) + offset0;
  const uint8_t* strend = reinterpret_cast<const uint8_t*>(col->strbuf) + offset1;
  const uint8_t* sch = strstart;
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
    std::memcpy(ch + 1, strstart, static_cast<size_t>(sch - strstart));
    *ch = '"';
    ch += sch - strstart + 1;
    while (sch < strend) {
      if (*sch == '"') *ch++ = '"';  // double the quote
      *ch++ = static_cast<char>(*sch++);
    }
    *ch++ = '"';
  }
  *pch = ch;
}


static char hexdigits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
static void write_f8_hex(char** pch, CsvColumn* col, size_t row)
{
  // Read the value as if it was uint64_t
  uint64_t value = static_cast<const uint64_t*>(col->data)[row];
  char *ch = *pch;

  if (value & F64_SIGN_MASK) {
    *ch++ = '-';
    value ^= F64_SIGN_MASK;
  }

  int exp = static_cast<int>(value >> 52);
  int subnormal = (exp == 0);
  if (exp == 0x7FF) {  // nan & inf
    if (value == F64_INFINITY) {  // minus sign was already printed, if any
      *ch++ = 'i';
      *ch++ = 'n';
      *ch++ = 'f';
      *pch = ch;
    } else {
      // do not print anything for nans
    }
    return;
  }
  uint64_t sig = (value & 0xFFFFFFFFFFFFF);
  ch[0] = '0';
  ch[1] = 'x';
  ch[2] = '1' - static_cast<char>(subnormal);
  ch[3] = '.';
  ch += 3 + (sig != 0);
  while (sig) {
    uint64_t r = sig & 0xF000000000000;
    *ch++ = hexdigits[r >> 48];
    sig = (sig ^ r) << 4;
  }
  // Add the exponent bias. Special treatment for subnormals (exp==0, value>0)
  // which should be encoded with exp=-1022, and zero (exp==0, value==0) which
  // should be encoded with exp=0.
  // `val & -flag` is equivalent to `flag? val : 0` if `flag` is 0 / 1.
  exp = (exp - 1023 + subnormal) & -(value != 0);
  *ch++ = 'p';
  *ch++ = '+' + (exp < 0)*('-' - '+');
  itoa(&ch, abs(exp));
  *pch = ch;
}


static void write_f4_hex(char** pch, CsvColumn* col, size_t row)
{
  // Read the value as if it was uint32_t
  uint32_t value = static_cast<const uint32_t*>(col->data)[row];
  char *ch = *pch;

  if (value & F32_SIGN_MASK) {
    *ch++ = '-';
    value ^= F32_SIGN_MASK;
  }

  int exp = static_cast<int>(value >> 23);
  int subnormal = (exp == 0);
  if (exp == 0xFF) {  // nan & inf
    if (value == F32_INFINITY) {  // minus sign was already printed, if any
      *ch++ = 'i';
      *ch++ = 'n';
      *ch++ = 'f';
      *pch = ch;
    }
    return;
  }
  uint32_t sig = (value & 0x7FFFFF);
  ch[0] = '0';
  ch[1] = 'x';
  ch[2] = '1' - static_cast<char>(subnormal);
  ch[3] = '.';
  ch += 3 + (sig != 0);
  while (sig) {
    uint32_t r = sig & 0x780000;
    *ch++ = hexdigits[r >> 19];
    sig = (sig ^ r) << 4;
  }
  exp = (exp - 127 + subnormal) & -(value != 0);
  *ch++ = 'p';
  *ch++ = '+' + (exp < 0)*('-' - '+');
  itoa(&ch, abs(exp));
  *pch = ch;
}


static void write_f8_dec(char** pch, CsvColumn* col, size_t row) {
  double value = static_cast<const double*>(col->data)[row];
  dtoa(pch, value);
}

static void write_f4_dec(char** pch, CsvColumn* col, size_t row) {
  float value = static_cast<const float*>(col->data)[row];
  ftoa(pch, value);
}


/**
 * This function writes a plain 0-terminated C string. This is not a regular
 * "writer" -- instead it may be used to write extra data to the file, such
 * as headers/footers.
 */
static void write_string(char** pch, const char *value)
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
    fixed_size_per_row(0),
    t_last(0)
{}


CsvWriter::~CsvWriter()
{
  for (size_t i = 0; i < columns.size(); i++)
    delete columns[i];
}


void CsvWriter::write()
{
  OmpExceptionManager oem;
  checkpoint();
  std::vector<RowColIndex> rcs = dt->split_columns_by_rowindices();
  RowIndex ri0 = rcs.size() == 1? rcs[0].rowindex : RowIndex();
  std::vector<size_t> colmapping(dt->ncols);
  for (size_t k = 0; k < rcs.size(); ++k) {
    for (size_t i : rcs[k].colindices) {
      colmapping[i] = k;
    }
  }

  size_t nstrcols32 = 0;
  size_t nstrcols64 = 0;
  size_t nrows = dt->nrows;
  size_t ncols = dt->ncols;
  size_t bytes_total = estimate_output_size();
  create_target(bytes_total);
  write_column_names();
  if (nrows == 0 || ncols == 0) goto finalize;

  determine_chunking_strategy(bytes_total, nrows);
  create_column_writers(ncols);
  nstrcols32 = strcolumns32.size();
  nstrcols64 = strcolumns64.size();

  // Start writing the CSV
  #pragma omp parallel num_threads(nthreads)
  {
    #pragma omp single
    {
      VLOG("Writing file using %zu chunks, with %.1f rows per chunk\n",
           static_cast<size_t>(nchunks), rows_per_chunk);
      VLOG("Using nthreads = %d\n", omp_get_num_threads());
      VLOG("Initial buffer size in each thread: %zu\n", bytes_per_chunk*2);
    }
    // Initialize thread-local variables
    size_t thbufsize = bytes_per_chunk * 2;
    char*  thbuf = nullptr;
    size_t th_write_at = 0;
    size_t th_write_size = 0;
    try {
      // Note: do not use new[] here, as it can't be safely realloced
      thbuf = dt::malloc<char>(thbufsize);
    } catch (...) {
      oem.capture_exception();
    }
    std::vector<size_t> js(rcs.size());

    // Main data-writing loop
    #pragma omp for ordered schedule(dynamic)
    for (size_t i = 0; i < nchunks; i++) {
      if (oem.exception_caught()) continue;
      size_t row0 = static_cast<size_t>(i * rows_per_chunk);
      size_t row1 = static_cast<size_t>((i + 1) * rows_per_chunk);
      if (i == nchunks-1) row1 = nrows;  // always go to the last row for last chunk

      try {
        // write the thread-local buffer into the output
        if (th_write_size) {
          wb->write_at(th_write_at, th_write_size, thbuf);
        }

        // Compute the required size of the thread-local buffer, and then
        // expand the buffer if necessary. The size of each column is multiplied
        // by 2 in order to account for the possibility that the buffer may
        // expand twice in size (if every character needs to be escaped).
        size_t reqsize = 0;
        for (size_t col = 0; col < nstrcols32; col++) {
          reqsize += strcolumns32[col]->strsize<uint32_t>(row0, row1);
        }
        for (size_t col = 0; col < nstrcols64; col++) {
          reqsize += strcolumns64[col]->strsize<uint64_t>(row0, row1);
        }
        reqsize *= 2;
        reqsize += fixed_size_per_row * static_cast<size_t>(row1 - row0);
        if (thbufsize < reqsize) {
          thbuf = static_cast<char*>(realloc(thbuf, reqsize));
          thbufsize = reqsize;
          if (!thbuf) {
            throw RuntimeError() << "Unable to allocate " << thbufsize
                                 << " bytes for thread-local buffer";
          }
        }

        // Write the data in rows row0..row1 and in all columns
        char* thch = thbuf;
        if (rcs.size() == 1) {
          ri0.iterate(row0, row1, 1,
            [&](size_t, size_t j) {
              if (j == RowIndex::NA) return;
              for (size_t col = 0; col < ncols; ++col) {
                columns[col]->write(&thch, j);
                *thch++ = ',';
              }
              thch[-1] = '\n';
            });
        } else {
          for (size_t row = row0; row < row1; ++row) {
            // Determine base row indices for the current row
            for (size_t k = 0; k < rcs.size(); ++k) {
              js[k] = rcs[k].rowindex[row];
            }
            // Run the serializer function
            for (size_t col = 0; col < ncols; ++col) {
              columns[col]->write(&thch, js[colmapping[col]]);
              *thch++ = ',';
            }
            thch[-1] = '\n';
          }
        }
        th_write_size = static_cast<size_t>(thch - thbuf);
        xassert(th_write_size <= thbufsize);
      } catch (...) {
        oem.capture_exception();
      }

      #pragma omp ordered
      {
        try {
          th_write_at = wb->prep_write(th_write_size, thbuf);
        } catch (...) {
          oem.capture_exception();
        }
      }
    }
    try {
      if (th_write_size && !oem.exception_caught()) {
        wb->write_at(th_write_at, th_write_size, thbuf);
      }
      dt::free(thbuf);
    } catch (...) {
      oem.capture_exception();
    }
  }
  finalize:
  oem.rethrow_exception_if_any();
  t_write_data = checkpoint();

  // Done writing; if writing to stdout then append '\0' to make it a regular
  // C string; otherwise truncate WritableBuffer to the final size.
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
  size_t nrows = dt->nrows;
  size_t ncols = dt->ncols;
  size_t total_string_size = 0;
  size_t total_columns_size = 0;
  fixed_size_per_row = ncols;  // 1 byte per separator
  for (size_t i = 0; i < ncols; i++) {
    Column *col = dt->columns[i];
    if (auto scol32 = dynamic_cast<StringColumn<uint32_t>*>(col)) {
      total_string_size += scol32->datasize();
    } else
    if (auto scol64 = dynamic_cast<StringColumn<uint64_t>*>(col)) {
      total_string_size += scol64->datasize();
    }
    SType stype = col->stype();
    fixed_size_per_row += bytes_per_stype[static_cast<int>(stype)];
    total_columns_size += column_names[i].size() + 1;
  }
  size_t bytes_total = fixed_size_per_row * nrows
                       + static_cast<size_t>(1.2 * total_string_size)
                       + total_columns_size;
  VLOG("  Estimated output size: %zu\n", bytes_total);
  t_size_estimation = checkpoint();
  return bytes_total;
}


/**
 * Create the target memory region (either in RAM, or on disk).
 */
void CsvWriter::create_target(size_t size) {
  wb = WritableBuffer::create_target(path, size, strategy);
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
    TRACK(ch0, maxsize, "CsvWriter.ch0");
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
    UNTRACK(ch0);
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
void CsvWriter::determine_chunking_strategy(size_t bytes_total, size_t nrows)
{
  xassert(nrows > 0);
  static constexpr size_t max_chunk_size = 1024 * 1024;
  static constexpr size_t min_chunk_size = 1024;

  nchunks = std::max(1 + (bytes_total - 1) / max_chunk_size,
                     nthreads == 1 ? 1 : nthreads*2);
  const double bytes_per_row = 1.0 * bytes_total / nrows;
  for (int i = 0; i < 5; ++i) {
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
      if (nthreads > nchunks) nthreads = nchunks;
      return;
    }
  }
  // This shouldn't really happen, but who knows...
  throw RuntimeError() << "Unable to determine how to write the file"
                       << ": bytes_total = " << bytes_total
                       << ", nrows = " << nrows
                       << ", nthreads = " << nthreads
                       << ", min.chunk = " << min_chunk_size
                       << ", max.chunk = " << max_chunk_size;
}


/**
 * Initialize writers for each column in the DataTable, i.e. fill in the vector
 * `columns` of `CsvColumn` objects. The objects created depend on the type of
 * each column, and on the options passed to the CsvWriter.
 */
void CsvWriter::create_column_writers(size_t ncols)
{
  columns.reserve(ncols);
  writers_per_stype[int(SType::FLOAT32)] = usehex? write_f4_hex : write_f4_dec;
  writers_per_stype[int(SType::FLOAT64)] = usehex? write_f8_hex : write_f8_dec;
  for (size_t i = 0; i < ncols; ++i) {
    Column* dtcol = dt->columns[i];
    SType stype = dtcol->stype();
    CsvColumn *csvcol = new CsvColumn(dtcol);
    columns.push_back(csvcol);
    if (stype == SType::STR32) strcolumns32.push_back(csvcol);
    if (stype == SType::STR64) strcolumns64.push_back(csvcol);
  }
  t_prepare_for_writing = checkpoint();
}



//=================================================================================================
// Helper functions
//=================================================================================================

void init_csvwrite_constants() {

  for (size_t i = 0; i < DT_STYPES_COUNT; i++) {
    bytes_per_stype[i] = 0;
    writers_per_stype[i] = nullptr;
  }
  bytes_per_stype[int(SType::BOOL)]    = 1;  // 1
  bytes_per_stype[int(SType::INT8)]    = 5;  // -100, -0x7F
  bytes_per_stype[int(SType::INT16)]   = 7;  // -32767, -0xFFFF
  bytes_per_stype[int(SType::INT32)]   = 11; // -2147483647, -0x7FFFFFFF
  bytes_per_stype[int(SType::INT64)]   = 20; // -9223372036854775807, -0x7FFFFFFFFFFFFFFF
  bytes_per_stype[int(SType::FLOAT32)] = 16; // -0x1.123456p+120 / -1.23456789e+37
  bytes_per_stype[int(SType::FLOAT64)] = 25; // -1.1234567890123457e+307, -0x1.23456789ABCDEp+1022
  bytes_per_stype[int(SType::STR32)]   = 2;  // ""
  bytes_per_stype[int(SType::STR64)]   = 2;  // ""

  writers_per_stype[int(SType::BOOL)]    = write_b1;
  writers_per_stype[int(SType::INT8)]    = write_iN<int8_t>;
  writers_per_stype[int(SType::INT16)]   = write_iN<int16_t>;
  writers_per_stype[int(SType::INT32)]   = write_iN<int32_t>;
  writers_per_stype[int(SType::INT64)]   = write_iN<int64_t>;
  writers_per_stype[int(SType::FLOAT32)] = write_f4_dec;
  writers_per_stype[int(SType::FLOAT64)] = write_f8_dec;
  writers_per_stype[int(SType::STR32)]   = write_str<uint32_t>;
  writers_per_stype[int(SType::STR64)]   = write_str<uint64_t>;
}
