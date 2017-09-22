#include <stdlib.h>
#include "Python.h"
#include "csv.h"
#include "myomp.h"
#include "utils.h"
#include "py_datatable.h"
#include "py_fread.h"
#include "py_utils.h"


PyObject* pywrite_csv(UU, PyObject *args)
{
  DataTable *dt = NULL;
  PyObject *tmp1 = NULL, *pydt = NULL;
  PyObject *csvwriter = NULL;
  char *filename = NULL;
  char **colnames = NULL;
  if (!PyArg_ParseTuple(args, "O:write_csv", &csvwriter))
    return NULL;

  printf("pywrite_csv(args=%p)\n", csvwriter);
  CsvWriteParameters *params = NULL;
  dtmalloc(params, CsvWriteParameters, 1);

  int nthreads = (int) TOINT64(ATTR(csvwriter, "nthreads"), 0);
  {
    int maxth = omp_get_max_threads();
    if (nthreads > maxth) nthreads = maxth;
    if (nthreads <= 0) nthreads += maxth;
    if (nthreads <= 0) nthreads = 1;
  }
  printf("  nthreads = %d\n", nthreads);

  pydt = PyObject_GetAttrString(csvwriter, "datatable");
  if (!dt_unwrap(pydt, &dt)) return NULL;
  printf("  datatable = %p  [%lld x %lld]\n", dt, dt->nrows, dt->ncols);

  filename = TOSTRING(ATTR(csvwriter, "path"), &tmp1);
  printf("  filename = %s\n", filename);
  colnames = TOSTRINGLIST(ATTR(csvwriter, "column_names"));
  printf("  colnames = ["); if (colnames){ for(int i=0; i<dt->ncols; i++) printf("%s, ", colnames[i]); printf("\b\b]\n"); } else printf("\bnull;\n");

  params->dt = dt;
  params->path = *filename? filename : NULL;
  params->nthreads = nthreads;
  params->column_names = colnames;

  // Write CSV
  try {
    printf("  calling csv_write()...\n");
    csv_write(params);
  } catch (...) {
    printf("  exception caught\n");
    return NULL;
  }

  return none();
  fail:
  return NULL;
}
