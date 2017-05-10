#include "py_evaluator.h"
#include "py_datatable.h"
#include "py_rowmapping.h"
#include "py_utils.h"


int init_py_evaluator(PyObject *module) {
    // Register Evaluator_PyType on the module
    Evaluator_PyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&Evaluator_PyType) < 0) return 0;
    Py_INCREF(&Evaluator_PyType);
    PyModule_AddObject(module, "Evaluator", (PyObject*) &Evaluator_PyType);

    return 1;
}


static PyObject* generate_stack(Evaluator_PyObject *self, PyObject *args)
{
    PyObject *list;

    if (!PyArg_ParseTuple(args, "O!:Evaluator.generate_stack",
                          &PyList_Type, &list))
        return NULL;

    if (self->stack != NULL || self->pystack != NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Stack has already been generated");
        return NULL;
    }

    self->pystack = incref(list);

    Py_ssize_t n = PyList_Size(list);
    if (n <= 0) {
        PyErr_SetString(PyExc_RuntimeError, "Stack is empty! cannot allocate");
        return NULL;
    }

    self->stack = calloc(sizeof(Value), (size_t) n);
    if (self->stack == NULL) return NULL;

    for (int i = 0; i < n; i++) {
        PyObject *item = PyList_GET_ITEM(list, i);
        if (item != Py_None) {
            if (PyObject_IsInstance(item, (PyObject*) &DataTable_PyType)) {
                self->stack[i].dt = ((DataTable_PyObject*) item)->ref;
            } else if (PyLong_Check(item)) {
                Py_ssize_t v = PyLong_AsLong(item);
                if (v > 0)
                    self->stack[i].ptr = malloc((size_t)v);
            } else {
                PyErr_SetString(PyExc_ValueError, "Unknown item on the stack");
                return NULL;
            }
            // PyList_SetItem(list, i, none());  // SET_ITEM will leak if used
        }
    }

    return none();
}


typedef void (mbr)(Value *stack, int64_t row0, int64_t row1);

static PyObject* run_mbr(Evaluator_PyObject *self, PyObject *args)
{
    void *fnptr;
    long long nrows;

    if (!PyArg_ParseTuple(args, "lL:Evaluator.run_mbr", &fnptr, &nrows))
        return NULL;

    ((mbr*)fnptr)(self->stack, 0, nrows);

    return none();
}


static PyObject* get_stack_value(Evaluator_PyObject *self, PyObject *args)
{
    int idx;
    int type;

    if (!PyArg_ParseTuple(args, "ii:Evaluator.get_stack_value", &idx, &type))
        return NULL;

    Value v = self->stack[idx];

    switch (type) {
        case 0: return PyLong_FromLongLong(v.i8);
        case 1: return PyLong_FromLong(v.i4);
        case 2: return PyLong_FromLong(v.i2);
        case 3: return PyLong_FromLong(v.i1);
        case 4: return PyFloat_FromDouble(v.f8);
        case 5: return PyFloat_FromDouble((double)v.f4);
        case 257: {
            int32_t n = v.i4;
            int32_t *arr = (int32_t*) self->stack[idx + 1].ptr;
            RowMapping *rwm = rowmapping_from_i32_array(arr, n);
            return (PyObject*) RowMappingPy_from_rowmapping(rwm);
        }
        case 258: {
            int64_t n = v.i8;
            int64_t *arr = (int64_t*) self->stack[idx + 1].ptr;
            RowMapping *rwm = rowmapping_from_i64_array(arr, (int64_t)n);
            return (PyObject*) RowMappingPy_from_rowmapping(rwm);
        }
        default:
            PyErr_Format(PyExc_ValueError, "Unsupported value type %d", type);
            return NULL;
    }
}



/**
 * Deallocator function, called when the object is being garbage-collected.
 */
static void __dealloc__(Evaluator_PyObject *self)
{
    free(self->stack);
    Py_XDECREF(self->pystack);
    Py_TYPE(self)->tp_free((PyObject*)self);
}



//======================================================================================================================
// DataTable type definition
//======================================================================================================================

PyDoc_STRVAR(dtdoc_generate_stack, "Retrieve datatable's data within a window");
PyDoc_STRVAR(dtdoc_run_mbr, "Execute a function within evaluator's context");
PyDoc_STRVAR(dtdoc_get_stack_value, "Retrieve value at pos i on the stack");

#define METHOD1(name) {#name, (PyCFunction)name, METH_VARARGS, dtdoc_##name}

static PyMethodDef evaluator_methods[] = {
    METHOD1(generate_stack),
    METHOD1(run_mbr),
    METHOD1(get_stack_value),
    {NULL, NULL, 0, NULL}           /* sentinel */
};


PyTypeObject Evaluator_PyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_datatable.Evaluator",             /* tp_name */
    sizeof(Evaluator_PyObject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)__dealloc__,            /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash  */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    "Evaluator object",                 /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    evaluator_methods,                  /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    0,                                  /* tp_new */
    0,                                  /* tp_free */
    0,                                  /* tp_is_gc */
    0,                                  /* tp_bases */
    0,                                  /* tp_mro */
    0,                                  /* tp_cache */
    0,                                  /* tp_subclasses */
    0,                                  /* tp_weaklist */
    0,                                  /* tp_del */
    0,                                  /* tp_version_tag */
    0,                                  /* tp_finalize */
};
