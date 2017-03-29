// *****************************************************************************
//
//  This file is shared across `R data.table` and `Py datatable`.
//  R:  https://github.com/Rdatatable/data.table
//  Py: https://github.com/h2oai/datatable
//
// *****************************************************************************
#ifndef dt_FREAD_SHARED_H
#define dt_FREAD_SHARED_H
#include <stdlib.h>


typedef enum CharacterEncoding {
    CE_AUTO,
    CE_LATIN1,
    CE_UTF8,
    CE_UTF16BE,
    CE_UTF16LE,
    CE_GB18030,
    CE_BIG5,
    CE_SHIFTJIS,
} CharacterEncoding;


typedef enum FRColType {
  NEGATIVE = -1, // dummy to force signed int (sign bit is used as flag to save space when >10,000 columns)
  SXP_BOOL = 0,  // LGLSXP
  SXP_INT32,     // INTSXP
  SXP_INT64,     // REALSXP class "integer64" using package bit64
  SXP_DOUBLE,    // REALSXP
  SXP_STRING,    // STRSXP
  SXP_IGNORE     // NILSXP i.e. skip column (highest type so that all types can be bumped up to it by user)
} FRColType;

// This count doesn't include the dummy NEGATIVE type
#define NUMTYPES (SXP_IGNORE + 1)


typedef void (*colfilter_fun_t)(FRColType **, const char **, int64_t, void*);
typedef struct FReadExtraArgs;


// *****************************************************************************

typedef struct FReadArgs {
    // Name of the file to open: a \0-terminated C string. If the file name
    // contains non-ASCII characters, it should be UTF-8 encoded.
    const char *filename;

    // Data buffer: a \0-terminated C string. When this parameter is given,
    // fread() will read from the provided string. This parameter is exclusive
    // with `filename`.
    const char *input;

    // Character to use for a field separator. Multi-char separators are not
    // supported. If '\0' (default), then fread will autodetect it. A quotation
    // mark is not allowed as a separator.
    char sep;

    // Maximum number of rows to read, or 0 to read the entire dataset.
    int64_t nrows;

    // Is there a header at the beginning of the file? 0=no, 1=yes, 2=auto
    int header;

    // NULL-terminated list of strings that should be converted into NA values.
    const char **nastrings;

    // Number of input lines to skip when reading the file.
    int64_t skipLines;

    // Skip to the line containing this string. This parameter cannot be used
    // with `skipLines`.
    const char *skipToString;

    // If this function is not NULL, it will be called by fread() after it has
    // determined the basic structure of the file (i.e. names and types of all
    // columns). This function will be called as:
    //     filter_colums(&types, column_names, ncols, self)
    // and it has the opportunity to modify the types (or names) however it
    // wants. For example, the caller may:
    //     + set the type of certain column(s) to SXP_IGNORE
    //     + override the type with a user-provided option
    //     + modify column names
    colfilter_fun_t filter_columns;

    // Storage type for long integers (in R)
    FRColType integer64;

    // Decimal separator for numbers (defaults to '.')
    char dec;

    // Character to use as a quotation mark. The default is '"'. Pass '\0' to
    // disable field quoting.
    char quote;

    // Strip the whitespace from fields (default True).
    _Bool stripWhite;

    // If True, empty lines in the file will be skipped. Otherwise empty lines
    // will produce rows of NAs.
    _Bool skipEmptyLines;

    // If True, then rows are allowed to have variable number of columns, and
    // all ragged rows will be filled with NAs on the right.
    _Bool fill;

    // File encoding, auto-detected by default.
    CharacterEncoding encoding;

    // Emit extra "debug" information.
    _Bool verbose;

    // If True, then emit progress messages during the parsing.
    _Bool showProgress;

    // Any additional implementation-specific parameters.
    struct FReadExtraArgs *extra;

} FReadArgs;


// *****************************************************************************

int fread_main(FReadArgs *self, void *out);


#endif
