//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define CSV_PY_CSV_cc
#include "csv/py_csv.h"
#include "csv/reader.h"
#include "csv/writer.h"
#include <exception>
#include <vector>
#include <stdbool.h>
#include <stdlib.h>
#include "frame/py_frame.h"
#include "options.h"
#include "py_datatable.h"
#include "py_utils.h"
#include "utils.h"
#include "utils/parallel.h"


PyObject* write_csv(PyObject*, PyObject* args)
{
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "O:write_csv", &arg1)) return nullptr;
  py::robj pywr(arg1);

  DataTable* dt = pywr.get_attr("datatable").to_frame();
  auto filename = pywr.get_attr("path").to_string();
  auto strategy = pywr.get_attr("_strategy").to_string();
  auto sstrategy = (strategy == "mmap")  ? WritableBuffer::Strategy::Mmap :
                   (strategy == "write") ? WritableBuffer::Strategy::Write :
                                           WritableBuffer::Strategy::Auto;

  // Create the CsvWriter object
  CsvWriter cwriter(dt, filename);
  cwriter.set_logger(arg1);
  cwriter.set_verbose(pywr.get_attr("verbose").to_bool());
  cwriter.set_usehex(pywr.get_attr("hex").to_bool());
  cwriter.set_strategy(sstrategy);

  auto colnames = pywr.get_attr("column_names").to_stringlist();
  cwriter.set_column_names(colnames);  // move-assignment

  int32_t nthreads = pywr.get_attr("nthreads").to_int32();
  if (ISNA<int32_t>(nthreads)) {
    nthreads = config::nthreads;
  } else {
    int32_t maxth = omp_get_max_threads();
    if (nthreads > maxth) nthreads = maxth;
    if (nthreads <= 0) nthreads += maxth;
    if (nthreads <= 0) nthreads = 1;
  }
  cwriter.set_nthreads(static_cast<size_t>(nthreads));

  // Write CSV
  cwriter.write();

  // Post-process the result
  PyObject* result = nullptr;
  if (filename.empty()) {
    WritableBuffer *wb = cwriter.get_output_buffer();
    MemoryWritableBuffer *mb = dynamic_cast<MemoryWritableBuffer*>(wb);
    if (!mb) {
      throw RuntimeError() << "Unable to case WritableBuffer into "
                              "MemoryWritableBuffer";
    }
    // -1 because the buffer also stores trailing \0
    Py_ssize_t len = static_cast<Py_ssize_t>(mb->size() - 1);
    char *str = static_cast<char*>(mb->get_cptr());
    result = PyUnicode_FromStringAndSize(str, len);
  } else {
    result = none();
  }

  return result;
}


// Python API function which is a wrapper around GenericReader's functionality.
PyObject* gread(PyObject*, PyObject* args)
{
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "O:gread", &arg1)) return nullptr;
  py::robj pyreader(arg1);

  GenericReader rdr(pyreader);
  std::unique_ptr<DataTable> dtptr = rdr.read_all();
  return py::Frame::from_datatable(dtptr.release());
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
