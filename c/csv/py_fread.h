#ifndef dt_FREAD_IMPL_H
#define dt_FREAD_IMPL_H
#include <Python.h>
#include "memorybuf.h"
#include "csv/py_csv.h"


#define FREAD_MAIN_ARGS_EXTRA_FIELDS  PyObject *freader;



// Per-column per-thread temporary string buffers used to assemble processed
// string data. Length = `nstrcols`. Each element in this array has the
// following fields:
//     .buf -- memory region where all string data is stored.
//     .size -- allocation size of this memory buffer.
//     .ptr -- the `postprocessBuffer` stores here the total amount of string
//         data currently held in the buffer; while the `orderBuffer` function
//         puts here the offset within the global string buffer where the
//         current buffer should be copied to.
//     .idx8 -- index of the current column within the `buff8` array.
//     .idxdt -- index of the current column within the output DataTable.
//     .numuses -- synchronization lock. The purpose of this variable is to
//         prevent race conditions between threads that do memcpy, and another
//         thread that needs to realloc the underlying buffer. Without the lock,
//         if one thread is performing a mem-copy and the other thread wants to
//         reallocs the buffer, then the first thread will segfault in the
//         middle of its operation. In order to prevent this, we use this
//         `.numuses` variable: when positive it shows the number of threads
//         that are currently writing to the same buffer. However when this
//         variable is negative, it means the buffer is being realloced, and no
//         other threads is allowed to initiate a memcopy.
//
typedef struct StrBuf {
    MemoryBuffer* mbuf;
    //char *buf;
    //size_t size;
    size_t ptr;
    int idx8;
    int idxdt;
    volatile int numuses;
    int _padding;
} StrBuf;

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



#endif
