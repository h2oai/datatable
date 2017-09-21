#include <errno.h>      // errno
#include <fcntl.h>      // open
#include <stdint.h>     // int32_t, etc
#include <string.h>     // strerror
#include <sys/mman.h>   // mmap
#include "datatable.h"
#include "myomp.h"
#include "types.h"

typedef void (*writer_fn)(char **pch, Column* col, int64_t row);

static int64_t bytes_per_stype[DT_STYPES_COUNT];
static writer_fn writers_per_stype[DT_STYPES_COUNT];
static void* create_file_and_mmap(const char *filename, int64_t filesize);

// Helper lookup table for writing integers
static const int32_t DIVS10[10]
  = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};



//=================================================================================================
// Field writers
//
// Note: we attempt to optimize these functions for speed. See /microbench/writecsv for various
// experiments and benchmarks.
//=================================================================================================

static void write_b1(char **pch, Column *col, int64_t row)
{
  int8_t value = ((int8_t*) col->data)[row];
  **pch = (char)value + '0';
  *pch += (value != NA_I1);
}


static void write_i1(char **pch, Column *col, int64_t row)
{
  int value = (int) ((int8_t*) col->data)[row];
  char *ch = *pch;
  if (value < 0) {
    if (value == NA_I1) return;
    *ch++ = '-';
    value = -value;
  }
  if (value >= 100) {  // the range of `value` is up to 127
    *ch++ = '1';
    int d = value/10;
    *ch++ = d - 10 + '0';
    value -= d*10;
  } else if (value >= 10) {
    int d = value/10;
    *ch++ = d + '0';
    value -= d*10;
  }
  *ch++ = value + '0';
  *pch = ch;
}


static void write_i2(char **pch, Column *col, int64_t row) {
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
    *ch++ = d + '0';
    value -= d * DIVS10[r];
  }
  *ch = value + '0';
  *pch = ch + 1;
}


static void write_i4(char **pch, Column *col, int64_t row)
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
    *ch++ = d + '0';
    value -= d * DIVS10[r];
  }
  *ch = value + '0';
  *pch = ch + 1;
}


static void write_s4(char **pch, Column *col, int64_t row)
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
    memcpy(ch+1, strstart, sch - strstart);
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
static void write_f8(char **pch, Column *col, int64_t row) {
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



//=================================================================================================
//
// Main CSV-writing function
//
//=================================================================================================
int csv_write(CsvWriteParameters *args)
{
  // Parse arguments
  DataTable *dt = args->dt;
  int nthreads = args->nthreads;
  {
    int maxth = omp_get_max_threads();
    if (nthreads > maxth) nthreads = maxth;
    if (nthreads <= 0) nthreads += maxth;
    if (nthreads <= 0) nthreads = 1;
  }
  char sep = args.sep;


  // First, estimate the size of the output CSV file
  int64_t nrows = dt->nrows;
  int64_t ncols = dt->ncols;
  int64_t bytes_total = 0;
  for (int64_t i = 0; i < ncols; i++) {
    SType stype = dt->columns[i]->stype;
    bytes_total += stype == ST_STRING_I4_VCHAR ? column_i4s_datasize(dt->columns[i]) + 2 * nrows :
                   stype == ST_STRING_I8_VCHAR ? column_i8s_datasize(dt->columns[i]) + 2 * nrows :
                   bytes_per_stype[stype] * nrows;
  }
  bytes_total += ncols * nrows;  // Account for separators / newlines
  double bytes_per_row = (double)bytes_total / nrows;

  // Open/create the file, and memory-map it
  void *mmp = create_file_and_mmap(ags.path, bytes_total);

  // Prepare writing environment and calculate best parameters
  int64_t nchunks = nthreads * 2;
  bool stopTeam = false;
  writer_fn *writers = NULL;
  dtmalloc(writers, writer_fn, ncols);
  for (int64_t i = 0; i < ncols; i++) {
    writers[i] = writers_per_stype[dt->columns[i]->stype];
  }

  // Start writing the CSV
  #pragma omp parallel num_threads(nthreads)
  {
    size_t bufsize = (size_t)(bytes_per_row * rows_per_pchunk);
    char *thbuf = malloc(bufsize);
    Column **columns = dt->columns;

    #pragma omp for ordered schedule(dynamic)
    for (int64_t i = 0; i < nchunks; i++) {
      if (stopTeam) continue;
      int64_t row0 = i * rows_per_pchunk;
      int64_t row1 = row0 + rows_per_pchunk;
      if (row1 > nrows) row1 = nrows;
      char *tch = thbuf;

      for (int64_t row = row0; row < row1; row++) {
        for (int64_t col = 0; col < ncols; col++) {
          writers[col](tch, columns[col], row);
          *tch++ = sep;
        }
        *tch++ = '\n';
      }
    }
  }
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


/**
 * Create file `filename` of size `filesize`, then memory-map it for writing,
 * and finally return pointer to the memmapped region.
 * On failure, this function returns NULL.
 */
static void* create_file_and_mmap(const char *filename, int64_t filesize)
{
  // Create new file of size `filesize`.
  FILE *fp = fopen(filename, "w");
  fseek(fp, (long)(filesize - 1), SEEK_SET);
  fputc('\0', fp);
  fclose(fp);

  // Memory-map the file.
  int fd = open(filename, O_RDWR);
  void *buf = mmap(NULL, filesize, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
  if (buf == MAP_FAILED) {
    printf("Memory map failed with errno %d: %s\n", errno, strerror(errno));
    close(fd);
    return NULL;
  }
  close(fd);
  return buf;
}
