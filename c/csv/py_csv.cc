//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "csv/writer.h"
#include <exception>
#include <vector>
#include <stdbool.h>
#include <stdlib.h>
#include "Python.h"
#include "myomp.h"
#include "utils.h"
#include "py_datatable.h"
#include "py_fread.h"
#include "py_utils.h"


PyObject* pywrite_csv(PyObject*, PyObject* args)
{
  PyObject *pywriter = NULL;
  PyObject *result = NULL;
  if (!PyArg_ParseTuple(args, "O:write_csv", &pywriter)) return NULL;
  Py_INCREF(pywriter);

  try {
    DataTable* dt = get_attr_datatable(pywriter, "datatable");
    std::string filename = get_attr_string(pywriter, "path");
    std::string strategy = get_attr_string(pywriter, "_strategy");

    // Create the CsvWriter object
    CsvWriter cwriter(dt, filename);
    cwriter.set_logger(pywriter);
    cwriter.set_verbose(get_attr_bool(pywriter, "verbose"));
    cwriter.set_usehex(get_attr_bool(pywriter, "hex"));
    cwriter.set_strategy((strategy == "mmap")  ? WRITE_STRATEGY_MMAP :
                         (strategy == "write") ? WRITE_STRATEGY_WRITE :
                                                 WRITE_STRATEGY_AUTO);

    std::vector<std::string> colnames;
    get_attr_stringlist(pywriter, "column_names", colnames);
    cwriter.set_column_names(colnames);  // move-assignment

    int nthreads = static_cast<int>(get_attr_int64(pywriter, "nthreads"));
    int maxth = omp_get_max_threads();
    if (nthreads > maxth) nthreads = maxth;
    if (nthreads <= 0) nthreads += maxth;
    if (nthreads <= 0) nthreads = 1;
    cwriter.set_nthreads(nthreads);

    // Write CSV
    cwriter.write();

    // Post-process the result
    if (filename.empty()) {
      WritableBuffer *wb = cwriter.get_output_buffer();
      MemoryWritableBuffer *mb = dynamic_cast<MemoryWritableBuffer*>(wb);
      if (!mb) {
        throw Error("Unable to case WritableBuffer into MemoryWritableBuffer");
      }
      // -1 because the buffer also stores trailing \0
      Py_ssize_t len = static_cast<Py_ssize_t>(mb->size() - 1);
      char *str = static_cast<char*>(mb->get());
      result = PyUnicode_FromStringAndSize(str, len);
    } else {
      result = none();
    }

  } catch (const std::exception& e) {
    // If the error message was already set, then we don't want to overwrite it.
    // Otherwise, retrieve error message from the exception and pass to Python.
    if (!PyErr_Occurred())
      PyErr_Format(PyExc_RuntimeError, e.what());
  }
  Py_XDECREF(pywriter);
  return result;
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
