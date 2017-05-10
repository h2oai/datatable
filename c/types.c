#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>  // static_assert
#include "types.h"


//==============================================================================
// Static asserts
//==============================================================================

static_assert(INTPTR_MAX == INT64_MAX,
              "Only 64 bit platforms are supported.");

static_assert(sizeof(void*) == 8, "Expected size(void*) to be 8 bytes");
static_assert(sizeof(void*) == sizeof(size_t), "size(size_t) != size(void*)");
static_assert(sizeof(void*) == sizeof(int64_t), "size(int64_t) != size(void*)");

static_assert(sizeof(int8_t) == 1, "int8_t should be 1-byte");
static_assert(sizeof(int16_t) == 2, "int16_t should be 2-byte");
static_assert(sizeof(int32_t) == 4, "int32_t should be 4-byte");
static_assert(sizeof(int64_t) == 8, "int64_t should be 8-byte");
static_assert(sizeof(float) == 4, "float should be 4-byte");
static_assert(sizeof(double) == 8, "double should be 8-byte");

static_assert(sizeof(LType) == 1, "LType does not fit in a byte");
static_assert(sizeof(SType) == 1, "SType does not fit in a byte");



//==============================================================================
// NA handling
//==============================================================================

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



//==============================================================================
// Initialize auxiliary data structures
//==============================================================================

STypeInfo stype_info[DT_STYPES_COUNT];

void init_types(void)
{
    NA_F4 = ((float_repr){ .i = NA_F4_BITS }).f;
    NA_F8 = ((double_repr){ .i = NA_F8_BITS }).f;

    #define STI(T, code, csize, msize, vw, ltype) \
        stype_info[T] = (STypeInfo){code, csize, msize, vw, ltype};
    STI(ST_VOID,              "---", 0, 0,                   0, 0)
    STI(ST_BOOLEAN_I1,        "i1b", 1, 0,                   0, LT_BOOLEAN)
    STI(ST_INTEGER_I1,        "i1i", 1, 0,                   0, LT_INTEGER)
    STI(ST_INTEGER_I2,        "i2i", 2, 0,                   0, LT_INTEGER)
    STI(ST_INTEGER_I4,        "i4i", 4, 0,                   0, LT_INTEGER)
    STI(ST_INTEGER_I8,        "i8i", 8, 0,                   0, LT_INTEGER)
    STI(ST_REAL_F4,           "f4r", 4, 0,                   0, LT_REAL)
    STI(ST_REAL_F8,           "f8r", 8, 0,                   0, LT_REAL)
    STI(ST_REAL_I2,           "i2r", 2, sizeof(DecimalMeta), 0, LT_REAL)
    STI(ST_REAL_I4,           "i4r", 4, sizeof(DecimalMeta), 0, LT_REAL)
    STI(ST_REAL_I8,           "i8r", 8, sizeof(DecimalMeta), 0, LT_REAL)
    STI(ST_STRING_I4_VCHAR,   "i4s", 4, sizeof(VarcharMeta), 1, LT_STRING)
    STI(ST_STRING_I8_VCHAR,   "i8s", 8, sizeof(VarcharMeta), 1, LT_STRING)
    STI(ST_STRING_FCHAR,      "c#s", 0, sizeof(FixcharMeta), 0, LT_STRING)
    STI(ST_STRING_U1_ENUM,    "u1e", 1, sizeof(EnumMeta),    1, LT_STRING)
    STI(ST_STRING_U2_ENUM,    "u2e", 2, sizeof(EnumMeta),    1, LT_STRING)
    STI(ST_STRING_U4_ENUM,    "u4e", 4, sizeof(EnumMeta),    1, LT_STRING)
    STI(ST_DATETIME_I8_EPOCH, "i8d", 8, 0,                   0, LT_DATETIME)
    STI(ST_DATETIME_I8_PRTMN, "i8w", 8, 0,                   0, LT_DATETIME)
    STI(ST_DATETIME_I4_TIME,  "i4t", 4, 0,                   0, LT_DATETIME)
    STI(ST_DATETIME_I4_DATE,  "i4d", 4, 0,                   0, LT_DATETIME)
    STI(ST_DATETIME_I2_MONTH, "i2d", 2, 0,                   0, LT_DATETIME)
    STI(ST_OBJECT_PYPTR,      "p8p", 8, 0,                   0, LT_OBJECT)
    #undef STI

    //---- More static asserts -------------------------------------------------
    for (int i = -128; i <= 127; i++) {
        char ch = (char) i;
        int test1 = (ch >= '0' && ch <= '9');
        int test2 = ((uint_fast8_t)(ch - '0') < 10);
        assert(test1 == test2);
    }
}
