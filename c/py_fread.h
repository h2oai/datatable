#ifndef dt_FREAD_IMPL_H
#define dt_FREAD_IMPL_H
#include <Python.h>


#define FREAD_MAIN_ARGS_EXTRA_FIELDS  PyObject *freader;


typedef struct StrBuf {
    char *buf;
    size_t size;
    size_t ptr;
    size_t idx8;
} StrBuf;


// Per-column per-thread temporary string buffers used to assemble processed
// string data. Length = `nstrcols`. Each element in this array has the
// following fields:
//     .buf -- memory region where all string data is stored.
//     .size -- allocation size of this memory buffer.
//     .ptr -- the `postprocessBuffer` stores here the total amount of string
//         data currently held in the buffer; while the `orderBuffer` function
//         puts here the offset within the global string buffer where the
//         current buffer should be copied to.
//     .idx8 -- same as the global `.idx8`.
//
#define FREAD_PUSH_BUFFERS_EXTRA_FIELDS                                        \
    StrBuf *strbufs;                                                           \



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
