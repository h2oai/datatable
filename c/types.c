#include <assert.h>
#include "types.h"

STypeInfo stype_info[DT_STYPES_COUNT];


void init_types(void) {
    stype_info[DT_VOID]               = (STypeInfo){"---", 0, 0, 0};
    stype_info[DT_BOOLEAN_I8]         = (STypeInfo){"i1b", 1, 0, DT_BOOLEAN};
    stype_info[DT_INTEGER_I8]         = (STypeInfo){"i1i", 1, 0, DT_INTEGER};
    stype_info[DT_INTEGER_I16]        = (STypeInfo){"i2i", 2, 0, DT_INTEGER};
    stype_info[DT_INTEGER_I32]        = (STypeInfo){"i4i", 4, 0, DT_INTEGER};
    stype_info[DT_INTEGER_I64]        = (STypeInfo){"i8i", 8, 0, DT_INTEGER};
    stype_info[DT_REAL_F32]           = (STypeInfo){"f4r", 4, 0, DT_REAL};
    stype_info[DT_REAL_F64]           = (STypeInfo){"f8r", 8, 0, DT_REAL};
    stype_info[DT_REAL_I16]           = (STypeInfo){"i2r", 2, 1, DT_REAL};
    stype_info[DT_REAL_I32]           = (STypeInfo){"i4r", 4, 1, DT_REAL};
    stype_info[DT_REAL_I64]           = (STypeInfo){"i8r", 8, 1, DT_REAL};
    stype_info[DT_STRING_U32_VCHAR]   = (STypeInfo){"u4s", 4, 1, DT_STRING};
    stype_info[DT_STRING_U64_VCHAR]   = (STypeInfo){"u8s", 8, 1, DT_STRING};
    stype_info[DT_STRING_FCHAR]       = (STypeInfo){"c#s", 0, 1, DT_STRING};
    stype_info[DT_STRING_U8_ENUM]     = (STypeInfo){"u1e", 1, 1, DT_STRING};
    stype_info[DT_STRING_U16_ENUM]    = (STypeInfo){"u2e", 2, 1, DT_STRING};
    stype_info[DT_STRING_U32_ENUM]    = (STypeInfo){"u4e", 4, 1, DT_STRING};
    stype_info[DT_DATETIME_I64_EPOCH] = (STypeInfo){"i8d", 8, 0, DT_DATETIME};
    stype_info[DT_DATETIME_I64_PRTMN] = (STypeInfo){"i8w", 8, 0, DT_DATETIME};
    stype_info[DT_DATETIME_I32_TIME]  = (STypeInfo){"i4t", 4, 0, DT_DATETIME};
    stype_info[DT_DATETIME_I32_DATE]  = (STypeInfo){"i4d", 4, 0, DT_DATETIME};
    stype_info[DT_DATETIME_I16_MONTH] = (STypeInfo){"i2d", 2, 0, DT_DATETIME};
    stype_info[DT_OBJECT_PYPTR]       = (STypeInfo){"p8p", 8, 0, DT_OBJECT};

    assert(sizeof(char) == 1);
    assert(sizeof(int16_t) == 2);
    assert(sizeof(int32_t) == 4);
    assert(sizeof(int64_t) == 8);
    assert(sizeof(float) == 4);
    assert(sizeof(double) == 8);
    assert(sizeof(void*) == 8);
}
