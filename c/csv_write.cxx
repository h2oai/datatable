#include <algorithm>    // max
#include <errno.h>      // errno
#include <fcntl.h>      // open
#include <stdint.h>     // int32_t, etc
#include <string.h>     // strerror
#include <sys/mman.h>   // mmap
#include "column.h"
#include "csv.h"
#include "datatable.h"
#include "memorybuf.h"
#include "myomp.h"
#include "types.h"

class CsvColumn;
inline static void write_int32(char **pch, int32_t value);
static void write_string(char **pch, const char *value);

typedef void (*writer_fn)(char **pch, CsvColumn* col, int64_t row);

static const int64_t max_chunk_size = 1024 * 1024;
static const int64_t min_chunk_size = 1024;

static int64_t bytes_per_stype[DT_STYPES_COUNT];
static writer_fn writers_per_stype[DT_STYPES_COUNT];

// Helper lookup table for writing integers
static const int32_t DIVS10[10]
  = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

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
    if (col->stype == ST_STRING_I4_VCHAR) {
      strbuf = reinterpret_cast<char*>(data);
      data = strbuf + ((VarcharMeta*)col->meta)->offoff;
    }
  }

  void write(char **pch, int64_t row) {
    writer(pch, this, row);
  }
};


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
  if (value < 0) {
    if (value == NA_I1) return;
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
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I2) return;
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 1000)? 2 : 4;
  for (; value < DIVS10[r]; r--);
  for (; r; r--) {
    int d = value / DIVS10[r];
    *ch++ = static_cast<char>(d) + '0';
    value -= d * DIVS10[r];
  }
  *ch = static_cast<char>(value) + '0';
  *pch = ch + 1;
}


static void write_i4(char **pch, CsvColumn *col, int64_t row)
{
  int32_t value = ((int32_t*) col->data)[row];
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I4) return;
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 100000)? 4 : 9;
  for (; value < DIVS10[r]; r--);
  for (; r; r--) {
    int d = value / DIVS10[r];
    *ch++ = static_cast<char>(d) + '0';
    value -= d * DIVS10[r];
  }
  *ch = static_cast<char>(value) + '0';
  *pch = ch + 1;
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
  const char *strstart = col->strbuf + offset0;
  const char *strend = col->strbuf + offset1;
  const char *sch = strstart;
  while (sch < strend) {  // ',' is 44, '"' is 32
    char c = *sch;
    if ((uint8_t)c <= (uint8_t)',' && (c == ',' || c == '"' || (uint8_t)c < 32)) break;
    *ch++ = c;
    sch++;
  }
  if (sch < strend) {
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
static void write_f8(char **pch, CsvColumn *col, int64_t row)
{
  // Read the value as if it was double
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


/**
 * This function writes a plain 0-terminated C string. This is not a regular
 * "writer" -- instead it may be used to write extra data to the file, such
 * as headers/footers.
 */
static void write_string(char **pch, const char *value)
{
  char *ch = *pch;
  const char *sch = value;
  for (;;) {
    char c = *sch++;
    if (!c) { *pch = ch; return; }
    if (c == '"' || c == ',' || static_cast<uint8_t>(c) < 32) break;
    *ch++ = c;
  }
  // If we broke out of the loop above, it means we need to quote the field.
  // So, first rewind to the beginning
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

inline static void write_int32(char **pch, int32_t value)
{
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I4) return;
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 100000)? 4 : 9;
  for (; value < DIVS10[r]; r--);
  for (; r; r--) {
    int d = value / DIVS10[r];
    *ch++ = static_cast<char>(d) + '0';
    value -= d * DIVS10[r];
  }
  *ch = static_cast<char>(value) + '0';
  *pch = ch + 1;
}



//=================================================================================================
//
// Main CSV-writing function
//
//=================================================================================================
void csv_write(CsvWriteParameters *args)
{
  // Fetch arguments
  DataTable *dt = args->dt;
  int nthreads = args->nthreads;

  // First, estimate the size of the output CSV file
  // The size of string columns is estimated liberally, assuming it may
  // get inflated by no more than 20% (+2 chars for the quotes). If the data
  // contains many quotes, they may inflate more than this.
  // The size of numeric columns is estimated conservatively: we compute the
  // maximum amount of space that is theoretically required.
  // Overall, we will probably overestimate the final size of the CSV by a big
  // margin.
  int64_t nrows = dt->nrows;
  int64_t ncols = dt->ncols;
  int64_t bytes_total = 0;
  for (int64_t i = 0; i < ncols; i++) {
    Column *col = dt->columns[i];
    SType stype = col->stype;
    if (stype == ST_STRING_I4_VCHAR) {
      bytes_total += (int64_t)(1.2 * column_i4s_datasize(col)) + 2 * nrows;
    } else if (stype == ST_STRING_I8_VCHAR) {
      bytes_total += (int64_t)(1.2 * column_i8s_datasize(col)) + 2 * nrows;
    } else {
      bytes_total += bytes_per_stype[stype] * nrows;
    }
  }
  bytes_total += ncols * nrows;  // Account for separators / newlines
  double bytes_per_row = static_cast<double>(bytes_total / nrows);

  // Create the target memory region
  MemoryBuffer *mb = NULL;
  size_t allocsize = static_cast<size_t>(bytes_total);
  if (args->path) {
    mb = new MmapMemoryBuffer(args->path, allocsize, MB_CREATE|MB_EXTERNAL);
  } else {
    mb = new RamMemoryBuffer(allocsize);
  }
  size_t bytes_written = 0;

  // Write the column names
  char **colnames = args->column_names;
  if (colnames) {
    // printf("writing column names...\n");
    char *ch, *ch0, *colname;
    ch = ch0 = static_cast<char*>(mb->get());
    // printf("  buf = %p\n", ch);
    size_t maxsize = 0;
    while ((colname = *colnames++)) {
      // A string may expand up to twice in size (if all its characters need
      // to be escaped) + add 2 surrounding quotes + add a comma in the end.
      maxsize += strlen(colname)*2 + 2 + 1;
    }
    // printf("  maxsize = %zu\n", maxsize);
    mb->ensuresize(maxsize);
    // printf("  ensured\n");
    colnames = args->column_names;
    while ((colname = *colnames++)) {
      // printf("  writing %s\n", colname);
      write_string(&ch, colname);
      *ch++ = ',';
    }
    // Replace the last ',' with a newline
    ch[-1] = '\n';
    bytes_written += static_cast<size_t>(ch - ch0);
    // printf("  bytes written = %zu\n", bytes_written);
  }

  // Prepare writing environment and calculate best parameters
  bool stopTeam = false;
  int64_t nchunks = std::max((int64_t)(nthreads == 1? 1 : nthreads*2),
                             bytes_total/max_chunk_size);
  int64_t rows_per_chunk = nrows / nchunks;
  size_t bytes_per_chunk = (size_t)(bytes_per_row * rows_per_chunk);
  if (rows_per_chunk == 0) {
    // If each row's size is too large, then parse 1 row at a time.
    nchunks = nrows;
    rows_per_chunk = 1;
    bytes_per_chunk = static_cast<size_t>(bytes_per_row);
  }
  if (bytes_per_chunk < min_chunk_size) {
    // The data is too small, and number of available threads too large --
    // reduce the number of chunks so that we don't waster resources on
    // needless thread manipulation.
    nchunks = bytes_total / min_chunk_size;
    if (nchunks < 1) nchunks = 1;
    rows_per_chunk = nrows / nchunks;
    bytes_per_chunk = (size_t)(bytes_per_row * rows_per_chunk);
  }

  // Prepare columns for writing
  CsvColumn *columns = reinterpret_cast<CsvColumn*>(malloc((size_t)ncols * sizeof(CsvColumn)));
  for (int64_t i = 0; i < ncols; i++) {
    new (columns + i) CsvColumn(dt->columns[i]);
  }

  // Start writing the CSV
  #pragma omp parallel num_threads(nthreads)
  {
    size_t bufsize = bytes_per_chunk;
    char *thbuf = new char[bufsize];
    char *tch = thbuf;
    size_t th_write_at = 0;
    size_t th_write_size = 0;

    #pragma omp for ordered schedule(dynamic)
    for (int64_t i = 0; i < nchunks; i++) {
      if (stopTeam) continue;
      int64_t row0 = i * rows_per_chunk;
      int64_t row1 = row0 + rows_per_chunk;
      if (row1 > nrows) row1 = nrows;

      // write the thread-local buffer into the output
      if (th_write_size) {
        char *target = static_cast<char*>(mb->get()) + th_write_at;
        memcpy(target, thbuf, th_write_size);
        tch = thbuf;
        th_write_size = 0;
      }

      for (int64_t row = row0; row < row1; row++) {
        for (int64_t col = 0; col < ncols; col++) {
          columns[col].write(&tch, row);
          *tch++ = ',';
        }
        tch[-1] = '\n';
      }

      #pragma omp ordered
      {
        th_write_at = bytes_written;
        th_write_size = static_cast<size_t>(tch - thbuf);
        bytes_written += th_write_size;
        // TODO: Add check that the output buffer has enough space
      }
    }
    if (th_write_size) {
      char *target = static_cast<char*>(mb->get()) + th_write_at;
      memcpy(target, thbuf, th_write_size);
    }
    delete[] thbuf;
  }

  // Done writing; if writing to stdout then print the output buffer using the
  // plain C `printf()`; otherwise simply deleting the MemoryBuffer object
  // guarantees that the data will be stored to disk.
  if (args->path) {
    mb->resize(bytes_written);
  } else {
    char *target = static_cast<char*>(mb->get());
    target[bytes_written] = '\0';
    mb->resize(bytes_written + 1);
    printf("%s\n", target);
  }
  delete mb;
  free(columns);
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
  bytes_per_stype[ST_REAL_F8] = 24;    // -0x1.23456789ABCDEp+1000

  writers_per_stype[ST_BOOLEAN_I1] = (writer_fn) write_b1;
  writers_per_stype[ST_INTEGER_I1] = (writer_fn) write_i1;
  writers_per_stype[ST_INTEGER_I2] = (writer_fn) write_i2;
  writers_per_stype[ST_INTEGER_I4] = (writer_fn) write_i4;
  writers_per_stype[ST_REAL_F8] = (writer_fn) write_f8;
  writers_per_stype[ST_STRING_I4_VCHAR] = (writer_fn) write_s4;
}
