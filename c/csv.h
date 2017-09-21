#include <stdbool.h>
#include <stdint.h>
#include "datatable.h"


typedef struct CsvWriteParameters {

  // Output path where the DataTable should be written.
  const char *path;

  DataTable *dt;

  char **column_names;

  // Character to use as a separator between fields. Default ','. Only single-
  // character separators are supported
  char sep;

} CsvWriteParameters;


int csv_write(CsvWriteParameters *args);
