#include "fread_impl.h"


#define ATTR(pyobj, attr)  PyObject_GetAttrString(pyobj, attr)

#define TOSTRING(pyobj, dflt) ({                                               \
    char *res = dflt;                                                          \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        PyObject *y = PyUnicode_AsEncodedString(x, "utf-8", "strict");         \
        if (y == NULL) goto fail;                                              \
        res = PyBytes_AsString(y);                                             \
        Py_DECREF(y);                                                          \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})

#define TOFSSTRING(pyobj, dflt) ({                                             \
    char *res = dflt;                                                          \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        PyObject *y = PyUnicode_EncodeFSDefault(x);                            \
        if (y == NULL) goto fail;                                              \
        res = PyBytes_AsString(y);                                             \
        Py_DECREF(y);                                                          \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})

#define TOCHAR(pyobj, dflt) ({                                                 \
    char res = dflt;                                                           \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        res = (char)PyUnicode_ReadChar(x, 0);                                  \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})

#define TOINT64(pyobj, dflt) ({                                                \
    int64_t res = dflt;                                                        \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        res = PyLong_AsLongLong(x);                                            \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})

#define TOBOOL(pyobj, dflt) ({                                                 \
    int res = dflt;                                                            \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        res = (x == Py_True);                                                  \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})

#define TOSTRINGLIST(pyobj, dflt) ({                                           \
    char **res = dflt;                                                         \
    PyObject *x = pyobj;                                                       \
    if (x == NULL) goto fail;                                                  \
    if (x != Py_None) {                                                        \
        Py_ssize_t count = PyList_Size(x);                                     \
        res = calloc(sizeof(char*), (size_t)(count + 1));                      \
        for (Py_ssize_t i = 0; i < count; i++) {                               \
            PyObject *item = PyList_GetItem(x, i);                             \
            PyObject *y = PyUnicode_AsEncodedString(item, "utf-8", "strict");  \
            if (y == NULL) {                                                   \
                for (int j = 0; j < i; j++) free(res[j]);                      \
                free(res);                                                     \
                goto fail;                                                     \
            }                                                                  \
            res[i] = PyBytes_AsString(y);                                      \
            Py_DECREF(y);                                                      \
        }                                                                      \
    }                                                                          \
    Py_DECREF(x);                                                              \
    res;                                                                       \
})

static void free_string(const char *s) {
    union { const char *immut; char *mut; } u;
    u.immut = s;
    free(u.mut);
}



PyObject* freadPy(PyObject *self, PyObject *args)
{
    PyObject *freader;
    FReadArgs *frargs = NULL;
    FReadExtraArgs *extra = NULL;

    if (!PyArg_ParseTuple(args, "O:fread", &freader))
        goto fail;

    frargs = calloc(sizeof(frargs), 1);  // zero-out all pointers
    if (!frargs) goto fail;

    frargs->filename = TOFSSTRING(ATTR(freader, "filename"), NULL);
    frargs->input = TOSTRING(ATTR(freader, "text"), NULL);
    frargs->sep = TOCHAR(ATTR(freader, "separator"), '\0');
    frargs->nrows = TOINT64(ATTR(freader, "max_nrows"), 0);
    frargs->header = TOBOOL(ATTR(freader, "header"), 2);
    frargs->verbose = TOBOOL(ATTR(freader, "verbose"), 0);
    frargs->nastrings = TOSTRINGLIST(ATTR(freader, "na_strings"), NULL);

    extra = malloc(sizeof(FReadExtraArgs));
    extra->freader = freader;
    frargs->extra = extra;

    int res = fread_main(frargs, NULL);
    if (!res) goto fail;

    return PyLong_FromLong(res);

  fail:
    if (frargs) {
        if (frargs->nastrings) {
            char **ptr = frargs->nastrings;
            while (*ptr++) free(*ptr);
            free(frargs->nastrings);
        }
        free_string(frargs->filename);
        free_string(frargs->input);
        free(frargs->extra);
        free(frargs);
    }
    return NULL;
}
