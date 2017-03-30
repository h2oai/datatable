#include "fread_impl.h"


#define TRY(x) ({ PyObject *z = x; if (z == NULL) goto fail; z; })
#define XDECREF(x)      Py_XDECREF(x); x = NULL
#define XDECREF2(x, y)  Py_XDECREF(x); Py_XDECREF(y); x = y = NULL


PyObject* freadPy(PyObject *self, PyObject *args)
{
    PyObject *freader;
    FReadArgs *frargs = NULL;
    PyObject *a = NULL, *b = NULL;
    char **nastrings = NULL;
    FReadExtraArgs *extra = NULL;

    if (!PyArg_ParseTuple(args, "O:fread", &freader))
        goto fail;

    frargs = calloc(sizeof(FReadArgs), 1);  // zero-out all pointers
    if (!frargs) goto fail;

    a = TRY(PyObject_GetAttrString(freader, "filename"));
    if (a == Py_None) {
        frargs->filename = NULL;
    } else {
        b = TRY(PyUnicode_EncodeFSDefault(a));
        frargs->filename = PyBytes_AsString(b);
    }
    XDECREF2(a, b);

    a = TRY(PyObject_GetAttrString(freader, "text"));
    if (a == Py_None) {
        frargs->input = NULL;
    } else {
        b = TRY(PyUnicode_AsEncodedString(a, "utf-8", "strict"));
        frargs->input = PyBytes_AsString(b);
    }
    XDECREF2(a, b);


    a = TRY(PyObject_GetAttrString(freader, "separator"));
    frargs->sep = (a == Py_None)? 0 : (char)PyUnicode_ReadChar(a, 0);
    XDECREF(a);

    a = TRY(PyObject_GetAttrString(freader, "max_nrows"));
    frargs->nrows = (a == Py_None)? 0 : PyLong_AsLongLong(a);
    XDECREF(a);

    a = TRY(PyObject_GetAttrString(freader, "header"));
    frargs->header = (a == Py_False)? 0 : (a == Py_True)? 1 : 2;
    XDECREF(a);

    a = TRY(PyObject_GetAttrString(freader, "na_strings"));
    int n_nastrings = (int) PyList_Size(a);
    nastrings = malloc(sizeof(char**) * (size_t)(n_nastrings + 1));
    for (int i = 0; i < n_nastrings; i++) {
        PyObject *item = PyList_GetItem(a, i);  // borrowed reference
        b = TRY(PyUnicode_AsEncodedString(item, "utf-8", "strict"));
        nastrings[i] = PyBytes_AsString(b);
    }
    nastrings[n_nastrings] = NULL;
    frargs->nastrings = (const char* const*) nastrings;
    XDECREF2(a, b);

    a = TRY(PyObject_GetAttrString(freader, "verbose"));
    frargs->verbose = (a == Py_True);
    XDECREF(a);

    extra = malloc(sizeof(FReadExtraArgs));
    extra->freader = freader;
    frargs->extra = extra;

    int res = fread_main(frargs, NULL);
    if (!res) goto fail;

    return PyLong_FromLong(res);

  fail:
    if (nastrings) {
        char **ptr = nastrings;
        while (*ptr++) free(*ptr);
        free(nastrings);
    }
    free(extra);
    free(frargs);
    XDECREF2(a, b);
    return NULL;
}
