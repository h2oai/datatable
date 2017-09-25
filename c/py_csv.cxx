#include <exception>
#include <stdbool.h>
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

  try {
    CsvWriteParameters *params = NULL;
    dtmalloc(params, CsvWriteParameters, 1);

    bool verbose = get_attr_bool(csvwriter, "verbose");
    bool usehex = get_attr_bool(csvwriter, "hex");
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
    params->usehex = usehex;
    params->verbose = verbose;
    params->logger = csvwriter;

    // Write CSV
    MemoryBuffer *mb = csv_write(params);

    // Post-process the result
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
    // If the error message was already set, then we don't want to overwrite it.
    // Otherwise, retrieve error message from the exception and pass to Python.
    if (!PyErr_Occurred())
      PyErr_Format(PyExc_RuntimeError, e.what());
  }

  fail:
  return NULL;
}


__attribute__((format(printf, 2, 3)))
void log_message(void *logger, const char *format, ...) {
  static char msgstatic[2001];
  va_list args;
  va_start(args, format);
  char *msg = msgstatic;
  if (strcmp(format, "%s") == 0) {
    msg = va_arg(args, char*);
  } else {
    vsnprintf(msg, 2000, format, args);
  }
  va_end(args);
  PyObject_CallMethod(reinterpret_cast<PyObject*>(logger),
                      "_vlog", "O", PyUnicode_FromString(msg));
}
