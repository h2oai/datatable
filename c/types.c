#include <assert.h>
#include "types.h"

STypeInfo stype_info[DT_STYPES_COUNT];


void init_types(void) {
    stype_info[ST_VOID]               = (STypeInfo){"---", 0, 0, 0};
    stype_info[ST_BOOLEAN_I1]         = (STypeInfo){"i1b", 1, 0, LT_BOOLEAN};
    stype_info[ST_INTEGER_I1]         = (STypeInfo){"i1i", 1, 0, LT_INTEGER};
    stype_info[ST_INTEGER_I2]        = (STypeInfo){"i2i", 2, 0, LT_INTEGER};
    stype_info[ST_INTEGER_I4]        = (STypeInfo){"i4i", 4, 0, LT_INTEGER};
    stype_info[ST_INTEGER_I8]        = (STypeInfo){"i8i", 8, 0, LT_INTEGER};
    stype_info[ST_REAL_F4]           = (STypeInfo){"f4r", 4, 0, LT_REAL};
    stype_info[ST_REAL_F8]           = (STypeInfo){"f8r", 8, 0, LT_REAL};
    stype_info[ST_REAL_I2]           = (STypeInfo){"i2r", 2, 1, LT_REAL};
    stype_info[ST_REAL_I4]           = (STypeInfo){"i4r", 4, 1, LT_REAL};
    stype_info[ST_REAL_I8]           = (STypeInfo){"i8r", 8, 1, LT_REAL};
    stype_info[ST_STRING_I4_VCHAR]   = (STypeInfo){"i4s", 4, 1, LT_STRING};
    stype_info[ST_STRING_I8_VCHAR]   = (STypeInfo){"i8s", 8, 1, LT_STRING};
    stype_info[ST_STRING_FCHAR]       = (STypeInfo){"c#s", 0, 1, LT_STRING};
    stype_info[ST_STRING_U1_ENUM]     = (STypeInfo){"u1e", 1, 1, LT_STRING};
    stype_info[ST_STRING_U2_ENUM]    = (STypeInfo){"u2e", 2, 1, LT_STRING};
    stype_info[ST_STRING_U4_ENUM]    = (STypeInfo){"u4e", 4, 1, LT_STRING};
    stype_info[ST_DATETIME_I8_EPOCH] = (STypeInfo){"i8d", 8, 0, LT_DATETIME};
    stype_info[ST_DATETIME_I8_PRTMN] = (STypeInfo){"i8w", 8, 0, LT_DATETIME};
    stype_info[ST_DATETIME_I4_TIME]  = (STypeInfo){"i4t", 4, 0, LT_DATETIME};
    stype_info[ST_DATETIME_I4_DATE]  = (STypeInfo){"i4d", 4, 0, LT_DATETIME};
    stype_info[ST_DATETIME_I2_MONTH] = (STypeInfo){"i2d", 2, 0, LT_DATETIME};
    stype_info[DT_OBJECT_PYPTR]       = (STypeInfo){"p8p", 8, 0, LT_OBJECT};

    assert(sizeof(char) == 1);
    assert(sizeof(int16_t) == 2);
    assert(sizeof(int32_t) == 4);
    assert(sizeof(int64_t) == 8);
    assert(sizeof(float) == 4);
    assert(sizeof(double) == 8);
    assert(sizeof(void*) == 8);
}
