#include <exception>
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

  CsvWriteParameters *params = NULL;
  dtmalloc(params, CsvWriteParameters, 1);

  int nthreads = (int) TOINT64(ATTR(csvwriter, "nthreads"), 0);
  {
    int maxth = omp_get_max_threads();
    if (nthreads > maxth) nthreads = maxth;
    if (nthreads <= 0) nthreads += maxth;
    if (nthreads <= 0) nthreads = 1;
  }

  pydt = PyObject_GetAttrString(csvwriter, "datatable");
  if (!dt_unwrap(pydt, &dt)) return NULL;

  filename = TOSTRING(ATTR(csvwriter, "path"), &tmp1);
  if (filename && !*filename) filename = NULL;  // Empty string => NULL
  colnames = TOSTRINGLIST(ATTR(csvwriter, "column_names"));

  params->dt = dt;
  params->path = filename;
  params->nthreads = nthreads;
  params->column_names = colnames;

  // Write CSV
  try {
    MemoryBuffer *mb = csv_write(params);
    PyObject *res = NULL;
    if (filename) {
      res = none();
    } else {
      // -1 because the buffer also stores trailing \0
      Py_ssize_t len = static_cast<Py_ssize_t>(mb->size() - 1);
      char *str = reinterpret_cast<char*>(mb->get());
      res = PyUnicode_FromStringAndSize(str, len);
    }
    delete mb;
    return res;

  } catch (const std::exception& e) {
    PyErr_Format(PyExc_RuntimeError, e.what());
  }

  fail:
  return NULL;
}
