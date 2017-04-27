#include <assert.h>
#include "types.h"


STypeInfo stype_info[DT_STYPES_COUNT];


#define NA_F4_BITS 0x7F8007A2u
#define NA_F8_BITS 0x7FF00000000007A2ull
typedef union { uint32_t i; float f; } float_repr;
typedef union { uint64_t i; double f; } double_repr;
inline int ISNA_F4(float x) {
    float_repr xx;
    xx.f = x;
    return xx.i == NA_F4_BITS;
}
inline int ISNA_F8(double x) {
    double_repr xx;
    xx.f = x;
    return xx.i == NA_F8_BITS;
}

const int8_t   NA_I1 = INT8_MIN;
const int16_t  NA_I2 = INT16_MIN;
const int32_t  NA_I4 = INT32_MIN;
const int64_t  NA_I8 = INT64_MIN;
const uint8_t  NA_U1 = UINT8_MAX;
const uint16_t NA_U2 = UINT16_MAX;
const uint32_t NA_U4 = UINT32_MAX;
const uint64_t NA_U8 = UINT64_MAX;
float NA_F4;
double NA_F8;



void init_types(void)
{
    NA_F4 = ((float_repr){ .i = NA_F4_BITS }).f;
    NA_F8 = ((double_repr){ .i = NA_F8_BITS }).f;

    stype_info[ST_VOID]              = (STypeInfo){"---", 0, 0,                   0, 0};
    stype_info[ST_BOOLEAN_I1]        = (STypeInfo){"i1b", 1, 0,                   0, LT_BOOLEAN};
    stype_info[ST_INTEGER_I1]        = (STypeInfo){"i1i", 1, 0,                   0, LT_INTEGER};
    stype_info[ST_INTEGER_I2]        = (STypeInfo){"i2i", 2, 0,                   0, LT_INTEGER};
    stype_info[ST_INTEGER_I4]        = (STypeInfo){"i4i", 4, 0,                   0, LT_INTEGER};
    stype_info[ST_INTEGER_I8]        = (STypeInfo){"i8i", 8, 0,                   0, LT_INTEGER};
    stype_info[ST_REAL_F4]           = (STypeInfo){"f4r", 4, 0,                   0, LT_REAL};
    stype_info[ST_REAL_F8]           = (STypeInfo){"f8r", 8, 0,                   0, LT_REAL};
    stype_info[ST_REAL_I2]           = (STypeInfo){"i2r", 2, sizeof(DecimalMeta), 0, LT_REAL};
    stype_info[ST_REAL_I4]           = (STypeInfo){"i4r", 4, sizeof(DecimalMeta), 0, LT_REAL};
    stype_info[ST_REAL_I8]           = (STypeInfo){"i8r", 8, sizeof(DecimalMeta), 0, LT_REAL};
    stype_info[ST_STRING_I4_VCHAR]   = (STypeInfo){"i4s", 4, sizeof(VarcharMeta), 1, LT_STRING};
    stype_info[ST_STRING_I8_VCHAR]   = (STypeInfo){"i8s", 8, sizeof(VarcharMeta), 1, LT_STRING};
    stype_info[ST_STRING_FCHAR]      = (STypeInfo){"c#s", 0, sizeof(FixcharMeta), 0, LT_STRING};
    stype_info[ST_STRING_U1_ENUM]    = (STypeInfo){"u1e", 1, sizeof(EnumMeta),    1, LT_STRING};
    stype_info[ST_STRING_U2_ENUM]    = (STypeInfo){"u2e", 2, sizeof(EnumMeta),    1, LT_STRING};
    stype_info[ST_STRING_U4_ENUM]    = (STypeInfo){"u4e", 4, sizeof(EnumMeta),    1, LT_STRING};
    stype_info[ST_DATETIME_I8_EPOCH] = (STypeInfo){"i8d", 8, 0,                   0, LT_DATETIME};
    stype_info[ST_DATETIME_I8_PRTMN] = (STypeInfo){"i8w", 8, 0,                   0, LT_DATETIME};
    stype_info[ST_DATETIME_I4_TIME]  = (STypeInfo){"i4t", 4, 0,                   0, LT_DATETIME};
    stype_info[ST_DATETIME_I4_DATE]  = (STypeInfo){"i4d", 4, 0,                   0, LT_DATETIME};
    stype_info[ST_DATETIME_I2_MONTH] = (STypeInfo){"i2d", 2, 0,                   0, LT_DATETIME};
    stype_info[ST_OBJECT_PYPTR]      = (STypeInfo){"p8p", 8, 0,                   0, LT_OBJECT};

    assert(sizeof(char) == 1);
    assert(sizeof(int16_t) == 2);
    assert(sizeof(int32_t) == 4);
    assert(sizeof(int64_t) == 8);
    assert(sizeof(float) == 4);
    assert(sizeof(double) == 8);
    assert(sizeof(void*) == 8);
}
