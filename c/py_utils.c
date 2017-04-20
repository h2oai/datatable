#include <string.h>  // memcpy
#include "py_utils.h"


void* clone(void *src, size_t n_bytes) {
    void* copy = malloc(n_bytes);
    if (copy == NULL) {
        PyErr_Format(PyExc_RuntimeError,
            "Out of memory: unable to allocate %ld bytes", n_bytes);
        return NULL;
    }
    if (src != NULL) {
        memcpy(copy, src, n_bytes);
    }
    return copy;
}
