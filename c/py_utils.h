//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------


/*******************************************************************************
 * Collection of macros / helper functions to facilitate translation from
 * Python to C.
 ******************************************************************************/
#ifndef dt_PYUTILS_H
#define dt_PYUTILS_H
#include <Python.h>
#include "utils.h"
#include "utils/exceptions.h"

namespace config {
  extern PyObject* logger;
};
void log_call(const char* msg);


#define ES_FUNCTION(decl, call, log_msg)                                       \
  decl {                                                                       \
    try {                                                                      \
      if (config::logger) {                                                    \
        log_call("call: " log_msg);                                            \
        PyObject* res = call;                                                  \
        log_call("done: " log_msg);                                            \
        return res;                                                            \
      } else {                                                                 \
        return call;                                                           \
      }                                                                        \
    } catch (const std::exception& e) {                                        \
      if (config::logger) log_call("error: " log_msg);                         \
      exception_to_python(e);                                                  \
      return NULL;                                                             \
    }                                                                          \
  }



#define DECLARE_INFO(name, docstring)                                          \
  WHEN(HOMEFLAG,                                                               \
    static char cls_name[] = #name;                                            \
    static char cls_doc[] = docstring;                                         \
  )


#define DECLARE_METHOD(fn, docstring)                                          \
  DECLARE_METHOD4(fn, docstring, BASECLS, HOMEFLAG)

#define DECLARE_METHOD4(fn, docstring, basecls, homeflag)                      \
  PyObject* fn(basecls* self, PyObject* args);                                 \
  PyObject* fn##_safe(basecls*, PyObject*);                                    \
  extern char fn##_doc[];                                                      \
  extern char fn##_name[];                                                     \
  WHEN(homeflag,                                                               \
    char fn##_doc[] = docstring;                                               \
    char fn##_name[] = #fn;                                                    \
    ES_FUNCTION(                                                               \
      PyObject* fn##_safe(basecls* self, PyObject* args),                      \
      fn(self, args),                                                          \
      STRINGIFY(CLSNAME) "." #fn "(...)")                                      \
  )                                                                            \


#define DECLARE_FUNCTION(fn, docstring, homeflag)                              \
  DECLARE_METHOD4(fn, docstring, PyObject, homeflag)


#define DECLARE_GETTER(fn, docstring)                                          \
  PyObject* get_##fn(BASECLS* self);                                           \
  WHEN(HOMEFLAG,                                                               \
    static char doc_get_##fn[] = docstring;                                    \
    static char name_get_##fn[] = #fn;                                         \
    ES_FUNCTION(                                                               \
      static PyObject* safe_get_##fn(BASECLS* self, void*),                    \
      get_##fn(self),                                                          \
      STRINGIFY(CLSNAME) "." #fn)                                              \
  )                                                                            \


#define METHOD0(fn)                                                            \
  {fn##_name, (PyCFunction)fn##_safe, METH_NOARGS, fn##_doc}
#define METHODv(fn)                                                            \
  {fn##_name, (PyCFunction)fn##_safe, METH_VARARGS, fn##_doc}
#define METHODo(fn)                                                            \
  {fn##_name, (PyCFunction)fn##_safe, METH_O, fn##_doc}
#define GETTER(fn)                                                             \
  {name_get_##fn, (getter)safe_get_##fn, NULL, doc_get_##fn, NULL}
#define GETSET(fn)                                                             \
  {name_get_##fn, (getter)safe_get_##fn, (setter)safe_set_##fn,                \
   doc_get_##fn, NULL}


#define DT_DOCS(name, doc) \
    static char dtdoc_##name[] = doc; \
    static char dtvar_##name[] = #name;

#define DT_GETSETTER(name) \
    {dtvar_##name, (getter)get_##name, NULL, dtdoc_##name, NULL}

#define DT_METHOD1(name) \
    {dtvar_##name, (PyCFunction)meth_##name, METH_VARARGS, dtdoc_##name}


PyObject* none(void);
PyObject* incref(PyObject *x);
PyObject* decref(PyObject *x);

#define pyfree(ptr)  do { Py_XDECREF(ptr); ptr = NULL; } while(0)


#endif
