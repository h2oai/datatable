#include <assert.h>
#include "types.h"

STypeInfo stype_info[DT_STYPES_COUNT];


int init_types(void) {
    stype_info[DT_VOID]               = (STypeInfo){0, 0, 0};
    stype_info[DT_BOOLEAN_I8]         = (STypeInfo){1, 0, DT_BOOLEAN};
    stype_info[DT_INTEGER_I8]         = (STypeInfo){1, 0, DT_INTEGER};
    stype_info[DT_INTEGER_I16]        = (STypeInfo){2, 0, DT_INTEGER};
    stype_info[DT_INTEGER_I32]        = (STypeInfo){4, 0, DT_INTEGER};
    stype_info[DT_INTEGER_I64]        = (STypeInfo){8, 0, DT_INTEGER};
    stype_info[DT_REAL_F32]           = (STypeInfo){4, 0, DT_REAL};
    stype_info[DT_REAL_F64]           = (STypeInfo){8, 0, DT_REAL};
    stype_info[DT_REAL_I16]           = (STypeInfo){2, 1, DT_REAL};
    stype_info[DT_REAL_I32]           = (STypeInfo){4, 1, DT_REAL};
    stype_info[DT_REAL_I64]           = (STypeInfo){8, 1, DT_REAL};
    stype_info[DT_STRING_UI32_VCHAR]  = (STypeInfo){4, 1, DT_STRING};
    stype_info[DT_STRING_UI64_VCHAR]  = (STypeInfo){8, 1, DT_STRING};
    stype_info[DT_STRING_FCHAR]       = (STypeInfo){0, 1, DT_STRING};
    stype_info[DT_STRING_UI8_ENUM]    = (STypeInfo){1, 1, DT_STRING};
    stype_info[DT_STRING_UI16_ENUM]   = (STypeInfo){2, 1, DT_STRING};
    stype_info[DT_STRING_UI32_ENUM]   = (STypeInfo){4, 1, DT_STRING};
    stype_info[DT_DATETIME_I64_EPOCH] = (STypeInfo){8, 0, DT_DATETIME};
    stype_info[DT_DATETIME_I64_PRTMN] = (STypeInfo){8, 0, DT_DATETIME};
    stype_info[DT_DATETIME_I32_TIME]  = (STypeInfo){4, 0, DT_DATETIME};
    stype_info[DT_DATETIME_I32_DATE]  = (STypeInfo){4, 0, DT_DATETIME};
    stype_info[DT_DATETIME_I16_MONTH] = (STypeInfo){2, 0, DT_DATETIME};
    stype_info[DT_OBJECT_PYPTR]       = (STypeInfo){8, 0, DT_OBJECT};

    assert(sizeof(char) == 1);
    assert(sizeof(int16_t) == 2);
    assert(sizeof(int32_t) == 4);
    assert(sizeof(int64_t) == 8);
    assert(sizeof(float) == 4);
    assert(sizeof(double) == 8);
    assert(sizeof(void*) == 8);

    return 1;
}
