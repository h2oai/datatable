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
extern double logger_timer;
extern char logger_msg[];
void log_call(const char* msg);


#define ES_FUNCTION(decl, call, log_msg)                                       \
  decl {                                                                       \
    try {                                                                      \
      if (config::logger) {                                                    \
        snprintf(logger_msg, 1000, "call %s", log_msg);                        \
        log_call(logger_msg);                                                  \
        logger_timer = wallclock();                                            \
        PyObject* res = call;                                                  \
        snprintf(logger_msg, 1000, "done %s in %.3f ms",                       \
                 log_msg, (wallclock() - logger_timer) * 1000.0);              \
        log_call(logger_msg);                                                  \
        return res;                                                            \
      } else {                                                                 \
        return call;                                                           \
      }                                                                        \
    } catch (const std::exception& e) {                                        \
      if (config::logger) log_call("fail: " log_msg);                          \
      exception_to_python(e);                                                  \
      return NULL;                                                             \
    }                                                                          \
  }

#define ES_VOID_FUNCTION(decl, call, log_msg)                                  \
  decl {                                                                       \
    try {                                                                      \
      if (config::logger) {                                                    \
        snprintf(logger_msg, 1000, "call %s", log_msg);                        \
        log_call(logger_msg);                                                  \
        logger_timer = wallclock();                                            \
        call;                                                                  \
        snprintf(logger_msg, 1000, "done %s in %.3f ms",                       \
                 log_msg, (wallclock() - logger_timer) * 1000.0);              \
        log_call(logger_msg);                                                  \
      } else {                                                                 \
        call;                                                                  \
      }                                                                        \
    } catch (const std::exception& e) {                                        \
      if (config::logger) log_call("fail: " log_msg);                          \
      exception_to_python(e);                                                  \
    }                                                                          \
  }

#define ES_INT_FUNCTION(decl, call, log_msg)                                   \
  decl {                                                                       \
    try {                                                                      \
      if (config::logger) {                                                    \
        snprintf(logger_msg, 1000, "call %s", log_msg);                        \
        log_call(logger_msg);                                                  \
        logger_timer = wallclock();                                            \
        int res = call;                                                        \
        snprintf(logger_msg, 1000, "done %s in %.3f ms",                       \
                 log_msg, (wallclock() - logger_timer) * 1000.0);              \
        log_call(logger_msg);                                                  \
        return res;                                                            \
      } else {                                                                 \
        return call;                                                           \
      }                                                                        \
    } catch (const std::exception& e) {                                        \
      if (config::logger) log_call("fail: " log_msg);                          \
      exception_to_python(e);                                                  \
      return -1;                                                               \
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


#define DECLARE_GETSET(fn, docstring)                                          \
  PyObject* get_##fn(BASECLS* self);                                           \
  int set_##fn(BASECLS* self, PyObject* value);                                \
  WHEN(HOMEFLAG,                                                               \
    static char doc_get_##fn[] = docstring;                                    \
    static char name_get_##fn[] = #fn;                                         \
    ES_FUNCTION(                                                               \
      static PyObject* safe_get_##fn(BASECLS* self, void*),                    \
      get_##fn(self),                                                          \
      STRINGIFY(CLSNAME) "." #fn)                                              \
    ES_INT_FUNCTION(                                                           \
      static int safe_set_##fn(BASECLS* self, PyObject* value, void*),         \
      set_##fn(self, value),                                                   \
      STRINGIFY(CLSNAME) "." #fn)                                              \
  )                                                                            \


#define DECLARE_REPR()                                                         \
  WHEN(HOMEFLAG,                                                               \
    static PyObject* repr(BASECLS* self);                                      \
    ES_FUNCTION(                                                               \
      static PyObject* safe_repr(BASECLS* self),                               \
      repr(self),                                                              \
      "repr(" STRINGIFY(CLSNAME) ")")                                          \
  )


#define DECLARE_DESTRUCTOR()                                                   \
  WHEN(HOMEFLAG,                                                               \
    static void dealloc(BASECLS*);                                             \
    ES_VOID_FUNCTION(                                                          \
      static void safe_dealloc(BASECLS* self),                                 \
      dealloc(self),                                                           \
      "del " STRINGIFY(CLSNAME))                                               \
  )


#define DECLARE_CONSTRUCTOR()                                                  \
  WHEN(HOMEFLAG,                                                               \
    static int _init_(BASECLS*, PyObject*, PyObject*);                         \
    ES_INT_FUNCTION(                                                           \
      static int safe_init(BASECLS* self, PyObject* args, PyObject* kwds),     \
      _init_(self, args, kwds),                                                \
      "new " STRINGIFY(CLSNAME))                                               \
  )


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
#define DESTRUCTOR (destructor)safe_dealloc
#define CONSTRUCTOR (initproc)safe_init
#define REPR (reprfunc)safe_repr



PyObject* none(void);
PyObject* incref(PyObject *x);
PyObject* decref(PyObject *x);

#define pyfree(ptr)  do { Py_XDECREF(ptr); ptr = NULL; } while(0)


#endif
