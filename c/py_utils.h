/*******************************************************************************
 * Collection of macros / helper functions to facilitate translation from
 * Python to C.
 ******************************************************************************/
#ifndef dt_PYUTILS_H
#define dt_PYUTILS_H
#include <Python.h>
#include "utils.h"
#include "utils/exceptions.h"

#define ES_FUNCTION(decl, call)                                                \
  decl {                                                                       \
    try {                                                                      \
      return call;                                                             \
    } catch (Error& err) {                                                     \
      err.topython();                                                          \
    } catch (std::exception& e) {                                              \
      (Error() << e.what()).topython();                                        \
    }                                                                          \
    return nullptr;                                                            \
  }



#define DECLARE_API_FUNCTION(fn, docstring, home)                              \
  DECLARE_METHOD(fn, PyObject, docstring, home)


#define DECLARE_METHOD(fn, base, docstring, home)                              \
  PyObject* fn(base* self, PyObject* args);                                    \
  WHEN(home,                                                                   \
    static char doc_##fn[] = docstring;                                        \
    static char name_##fn[] = #fn;                                             \
    ES_FUNCTION(                                                               \
      static PyObject* safe_##fn(base* self, PyObject* args),                  \
      fn(self, args))                                                          \
  )                                                                            \

#define DECLARE_GETTER(fn, base, docstring, home)                              \
  PyObject* get_##fn(base* self);                                              \
  WHEN(home,                                                                   \
    static char doc_get_##fn[] = docstring;                                    \
    static char name_get_##fn[] = #fn;                                         \
    ES_FUNCTION(                                                               \
      static PyObject* safe_get_##fn(base* self, void*),                       \
      get_##fn(self))                                                          \
  )                                                                            \


#define METHOD0(fn)                                                            \
  {name_##fn, (PyCFunction)safe_##fn, METH_NOARGS, doc_##fn}
#define METHODv(fn)                                                            \
  {name_##fn, (PyCFunction)safe_##fn, METH_VARARGS, doc_##fn}
#define METHODo(fn)                                                            \
  {name_##fn, (PyCFunction)safe_##fn, METH_O, doc_##fn}
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
