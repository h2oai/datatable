#include "py_types.h"


PyObject* py_ltype_names[DT_LTYPES_COUNT];
PyObject* py_stype_names[DT_STYPES_COUNT];


int init_py_types(PyObject *module)
{
    init_types();

    py_ltype_names[DT_MU]       = PyUnicode_FromString("mu");
    py_ltype_names[DT_BOOLEAN]  = PyUnicode_FromString("bool");
    py_ltype_names[DT_INTEGER]  = PyUnicode_FromString("int");
    py_ltype_names[DT_REAL]     = PyUnicode_FromString("real");
    py_ltype_names[DT_STRING]   = PyUnicode_FromString("str");
    py_ltype_names[DT_DATETIME] = PyUnicode_FromString("time");
    py_ltype_names[DT_DURATION] = PyUnicode_FromString("duration");
    py_ltype_names[DT_OBJECT]   = PyUnicode_FromString("obj");

    for (int i = 0; i < DT_LTYPES_COUNT; i++) {
        if (py_ltype_names[i] == NULL) return 0;
    }

    for (int i = 0; i < DT_STYPES_COUNT; i++) {
        py_stype_names[i] = PyUnicode_FromString(stype_info[i].code);
        if (py_stype_names[i] == NULL) return 0;
    }

    return 1;
}
