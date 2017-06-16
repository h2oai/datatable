#ifndef dt_FREAD_IMPL_H
#define dt_FREAD_IMPL_H
#include <Python.h>


#define FREAD_MAIN_ARGS_EXTRA_FIELDS  PyObject *freader;


#define FREAD_PUSH_BUFFERS_EXTRA_FIELDS


// Exception-raising macro for `fread()`, which renames it into "STOP". Usage:
//     if (cond) STOP("Bad things happened: %s", smth_bad);
//
#define STOP(...)                                                              \
    do {                                                                       \
        PyErr_Format(PyExc_RuntimeError, __VA_ARGS__);                         \
        freadCleanup();                                                        \
        return 0;                                                              \
    } while(0)


// This macro raises a warning using Python's standard warning mechanism. Usage:
//     if (cond) WARN("Attention: %s", smth_smelly);
//
#define DTWARN(...)                                                            \
    do {                                                                       \
        PyErr_WarnFormat(PyExc_RuntimeWarning, 1, __VA_ARGS__);                \
        if (warningsAreErrors) {                                               \
            freadCleanup();                                                    \
            return 0;                                                          \
        }                                                                      \
    } while(0)


// Print message to the output log. Usage:
//     DTPRINT("Nothing suspicious here, move along...");
//
void DTPRINT(const char *format, ...) __attribute__((format(printf, 1, 2)));



PyObject* freadPy(PyObject *self, PyObject *args);


#endif
