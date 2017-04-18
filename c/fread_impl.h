#ifndef dt_FREAD_IMPL_H
#define dt_FREAD_IMPL_H
#include <Python.h>
#include "fread.h"


typedef struct FReadExtraArgs {
    PyObject *freader;
} FReadExtraArgs;


// Exception-raising macro for `fread()`, which renames it into "STOP". Usage:
//     if (cond) STOP("Bad things happened: %s", smth_bad);
//
#define STOP(...) \
    { \
        PyErr_Format(PyExc_RuntimeError, __VA_ARGS__); \
        freadCleanup(); \
        return 0; \
    }


// This macro raises a warning using Python's standard warning mechanism. Usage:
//     if (cond) WARN("Attention: %s", smth_smelly);
//
#define DTWARN(...) \
    { \
        PyErr_WarnFormat(PyExc_RuntimeWarning, 1, __VA_ARGS__); \
    }


// Log a message, but only in "verbose" mode. Usage:
//     VLOG("Nothing suspicious here, move along...");
// The macro assumes that variables `verbose` and `args` are present in the
// local scope.
#define DTPRINT(...) log_message(args.freader, __VA_ARGS__)



PyObject* freadPy(PyObject *self, PyObject *args);

void log_message(PyObject* freader, const char *format, ...);


#endif
