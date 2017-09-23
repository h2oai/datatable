#include <stdbool.h>
#include <stdint.h>
#include "datatable.h"
#include "memorybuf.h"


typedef struct CsvWriteParameters {

  // Output path where the DataTable should be written.
  const char *path;

  DataTable *dt;

  char **column_names;

  int nthreads;

  int _padding;

} CsvWriteParameters;


MemoryBuffer* csv_write(CsvWriteParameters *args);
void init_csvwrite_constants();
