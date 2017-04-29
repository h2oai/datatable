// *****************************************************************************
//
//  This file is shared across `R data.table` and `Py datatable`.
//  R:  https://github.com/Rdatatable/data.table
//  Py: https://github.com/h2oai/datatable
//
// *****************************************************************************
#ifndef dt_FREAD_H
#define dt_FREAD_H
#include <stdint.h>  // uint32_t
#include <stdlib.h>  // size_t
#include "fread_impl.h"


// Ordered hierarchy of types
typedef enum {
  NEG = -1,       // dummy to force signed type; sign bit used for out-of-sample type bump management
  CT_DROP = 0,    // skip column requested by user; it is navigated as a string column with the prevailing quoteRule
  CT_BOOL8,       // int8_t; first type enum value must be 1 not 0 so that it can be negated to -1.
  CT_INT32_BARE,  // int32_t bare bones fast
  CT_INT32_FULL,  // int32_t if spaces or quotes can surround the value
  CT_INT64,       // int64_t
  CT_FLOAT64,     // double (64-bit IEEE 754 float)
  CT_STRING,      // lenOff struct below
  NUMTYPE         // placeholder for the number of types including drop; used for allocation and loop bounds
} colType;

extern size_t typeSize[NUMTYPE];
extern const char typeName[NUMTYPE][10];
extern const long double pow10lookup[701];


// Strings are pushed by fread_main using an offset from an anchor address plus
// string length; fread_impl.c then manages strings appropriately
typedef struct {
  int32_t len;  // signed to distinguish NA vs empty ""
  uint32_t off;
} lenOff;


#define NA_BOOL8         INT8_MIN
#define NA_INT32         INT32_MIN
#define NA_INT64         INT64_MIN
#define NA_FLOAT64_I64   0x7FF00000000007A2
#define NA_LENOFF        INT32_MIN  // lenOff.len only; lenOff.off undefined for NA



// *****************************************************************************

typedef struct freadMainArgs
{
  // Name of the file to open (a \0-terminated C string). If the file name
  // contains non-ASCII characters, it should be UTF-8 encoded (however fread
  // will not validate the encoding).
  const char *filename;

  // Data buffer: a \0-terminated C string. When this parameter is given,
  // fread() will read from the provided string. This parameter is exclusive
  // with `filename`.
  const char *input;

  // Character to use for a field separator. Multi-character separators are not
  // supported. If `sep` is '\0', then fread will autodetect it. A quotation
  // mark '"' is not allowed as field separator.
  char sep;

  // Decimal separator for numbers (usually '.'). This may coincide with `sep`,
  // in which case floating-point numbers will have to be quoted. Multi-char
  // (or non-ASCII) decimal separators are not supported. A quotation mark '"'
  // is not allowed as decimal separator.
  // See: https://en.wikipedia.org/wiki/Decimal_mark
  char dec;

  // Character to use as a quotation mark (usually '"'). Pass '\0' to disable
  // field quoting. This parameter cannot be auto-detected. Multi-character,
  // non-ASCII, or different open/closing quotation marks are not supported.
  char quote;

  // Is there a header at the beginning of the file?
  // 0 = no, 1 = yes, -128 = autodetect
  int8_t header;

  // Maximum number of rows to read, or INT64_MAX to read the entire dataset.
  // Note that even if `nrowLimit = 0`, fread() will scan a sample of rows in
  // the file to detect column names and types (and other parsing settings).
  int64_t nrowLimit;

  // Number of input lines to skip when reading the file.
  int64_t skipNrow;

  // Skip to the line containing this string. This parameter cannot be used
  // with `skipLines`.
  const char *skipString;

  // NULL-terminated list of strings that should be converted into NA values.
  // The last entry in this array is NULL (sentinel), which lets us know where
  // the array ends.
  const char * const* NAstrings;

  // Strip the whitespace from fields (usually True).
  _Bool stripWhite;

  // If True, empty lines in the file will be skipped. Otherwise empty lines
  // will produce rows of NAs.
  _Bool skipEmptyLines;

  // If True, then rows are allowed to have variable number of columns, and
  // all ragged rows will be filled with NAs on the right.
  _Bool fill;

  // If True, then emit progress messages during the parsing.
  _Bool showProgress;

  // Maximum number of threads (should be >= 1).
  int32_t nth;

  // Emit extra debug-level information.
  _Bool verbose;

  // If true, then this field instructs `fread` to treat warnings as errors. In
  // particular in R this setting is turned on whenever `option(warn=2)` is set,
  // in which case calling the standard `warning()` raises an exception.
  // However `fread` still needs to know that the exception will be raised, so
  // that it can do proper cleanup / resource deallocation -- otherwise memory
  // leaks would occur.
  _Bool warningsAreErrors;


  // Any additional implementation-specific parameters.
  EXTRA_FIELDS

} freadMainArgs;



// *****************************************************************************

int freadMain(freadMainArgs __args);

/**
 * Opportunity to bump up types of columns, if the user so wishes (currently
 * bumping types down is not supported). The user may also mark some columns as
 * skipped.
 *
 * type: detected column types
 * ncol: total number of columns
 * colNames: may be null if there are no col names
 * return: false = stop reading file
 */
_Bool userOverride(int8_t *type, lenOff *colNames, const char *anchor, int ncol);


/**
 * type: array of column types (i.e. colType), as signed chars.
 * ncols: number of elements in the array
 * ndrop: count of columns that have type[i] == CT_DROP (and should not be allocated)
 * nrows: number of rows in the datatable
 * return: total size of the Datatable created (for reporting purposes). If the
 *         return value is 0, then it indicates an error.
 */
size_t allocateDT(int8_t *type, int8_t *size, int ncols, int ndrop, uint64_t nrows);


/**
 * (code does not expect the values to remain)
 * col: col index (indexing into the final result)
 * colType: new type for the column
 */
void reallocColType(int col, colType newType);


/**
 * Called in-parallel from each thread
 * type: column types array
 * ncol: number of elemns in array `type`
 * buff: array of pointers to columns data-buffers (`ncol - ndrop` long)
 * anchor: buffer for string columns
 * nStringCols: number of columns of string types
 * nNonStringCols: number of columns of any other types
 *    (nStringCols + nNonStringCols == ncol - ndrop)
 * nRows: number of rows in the buffer
 * ansi: starting row where to put the data from the buffers into the final DT
 */
void pushBuffer(const void *buff, const char *anchor, int nRows,
                int64_t DTi, int rowsize,
                int nStringCols, int nNonStringCols);


/**
 * Called at the end to specify what the actual number of rows in the datatable
 * was. The function should adjust the datatable, reallocing the buffers if
 * necessary.
 */
void setFinalNrow(uint64_t nrows);


/**
 * Progress-report function.
 */
void progress(double percent/*[0,1]*/, double ETA/*secs*/);


void freadCleanup(void);

#endif

// If this function is not NULL, it will be called by fread() after it has
// determined the basic structure of the file (i.e. names and types of all
// columns). This function will be called as:
//     filter_colums(&types, column_names, ncols, self)
// and it has the opportunity to modify the types (or names) however it
// wants. For example, the caller may:
//     + set the type of certain column(s) to SXP_IGNORE
//     + override the type with a user-provided option
//     + modify column names
