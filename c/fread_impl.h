#ifndef dt_FREAD_H
#define dt_FREAD_H
#include <Python.h>
#include "fread.h"


typedef struct FReadExtraArgs {
    PyObject *freader;
} FReadExtraArgs;


// Exception-raising macro for `fread()`, which renames it into "STOP". Usage:
//     if (cond) STOP("Bad things happened: %s", smth_bad);
//
#define fread_STOP(format, ...) \
    { \
        PyErr_FormatString(format, __VA_ARGS__); \
        fread_onexit(); \
        return NULL; \
    }


// This macro raises a warning using Python's standard warning mechanism. Usage:
//     if (cond) WARN("Attention: %s", smth_smelly);
//
#define fread_WARN(format, ...) \
    { \
        PyErr_WarnFormat(&PyExc_RuntimeWarning, 1, format, __VA_ARGS__); \
    }


// Log a message, but only in "verbose" mode. Usage:
//     VLOG("Nothing suspicious here, move along...");
// The macro assumes that variables `verbose` and `args` are present in the
// local scope.
#define fread_VLOG(format, ...) \
    if (verbose) { \
        PyObject_CallMethod(args->extra->freader, "_vlog", "O", \
                            PyUnicode_FromFormat(format, __VA_ARGS__)); \
    }



PyObject* freadPy(PyObject *self, PyObject *args);

#endif
