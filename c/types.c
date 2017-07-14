#include "myassert.h"
#include "types.h"


//==============================================================================
// Static asserts
//==============================================================================

dt_static_assert(INTPTR_MAX == INT64_MAX,
                 "Only 64 bit platforms are supported.");

dt_static_assert(sizeof(void*) == 8, "Expected size(void*) to be 8 bytes");
dt_static_assert(sizeof(void*) == sizeof(size_t),
                 "size(size_t) != size(void*)");
dt_static_assert(sizeof(void*) == sizeof(int64_t),
                 "size(int64_t) != size(void*)");

dt_static_assert(sizeof(int8_t) == 1, "int8_t should be 1-byte");
dt_static_assert(sizeof(int16_t) == 2, "int16_t should be 2-byte");
dt_static_assert(sizeof(int32_t) == 4, "int32_t should be 4-byte");
dt_static_assert(sizeof(int64_t) == 8, "int64_t should be 8-byte");
dt_static_assert(sizeof(float) == 4, "float should be 4-byte");
dt_static_assert(sizeof(double) == 8, "double should be 8-byte");
dt_static_assert(sizeof(char) == sizeof(unsigned char), "char != uchar");
dt_static_assert(sizeof(void) == 1, "sizeof(void) != 1");
// Used in llvm.py
dt_static_assert(sizeof(long long int) == 8, "llint should be 8-byte");

dt_static_assert(sizeof(LType) == 1, "LType does not fit in a byte");
dt_static_assert(sizeof(SType) == 1, "SType does not fit in a byte");
dt_static_assert(sizeof(MType) == 1, "MType should be 1 byte in size");

dt_static_assert((unsigned)(-1) - (unsigned)(-3) == 2,
                 "Unsigned arithmetics check");
dt_static_assert(3u - (-1u) == 4u, "Unsigned arithmetics check");
dt_static_assert(-1u == 0xFFFFFFFFu, "Unsigned arithmetics check");



//==============================================================================
// NA handling
//==============================================================================

#define NA_F4_BITS 0x7F8007A2u
#define NA_F8_BITS 0x7FF00000000007A2ull
typedef union { uint32_t i; float f; } _flt;
typedef union { uint64_t i; double f; } _dbl;

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

inline int ISNA_F4(float x)    { return ((_flt){ .f = x }).i == NA_F4_BITS; }
inline int ISNA_F8(double x)   { return ((_dbl){ .f = x }).i == NA_F8_BITS; }
inline int ISNA_I1(int8_t x)   { return x == NA_I1; }
inline int ISNA_I2(int16_t x)  { return x == NA_I2; }
inline int ISNA_I4(int32_t x)  { return x == NA_I4; }
inline int ISNA_I8(int64_t x)  { return x == NA_I8; }
inline int ISNA_U1(uint8_t x)  { return x == NA_U1; }
inline int ISNA_U2(uint16_t x) { return x == NA_U2; }
inline int ISNA_U4(uint32_t x) { return x == NA_U4; }



//==============================================================================
// Initialize auxiliary data structures
//==============================================================================

STypeInfo stype_info[DT_STYPES_COUNT];

void init_types(void)
{
    NA_F4 = ((_flt){ .i = NA_F4_BITS }).f;
    NA_F8 = ((_dbl){ .i = NA_F8_BITS }).f;

    #define STI(T, code, csize, msize, vw, ltype, na) \
        stype_info[T] = (STypeInfo){csize, msize, na, code, ltype, vw, 0}
    STI(ST_VOID,              "---", 0, 0,                   0, 0,           NULL);
    STI(ST_BOOLEAN_I1,        "i1b", 1, 0,                   0, LT_BOOLEAN,  &NA_I1);
    STI(ST_INTEGER_I1,        "i1i", 1, 0,                   0, LT_INTEGER,  &NA_I1);
    STI(ST_INTEGER_I2,        "i2i", 2, 0,                   0, LT_INTEGER,  &NA_I2);
    STI(ST_INTEGER_I4,        "i4i", 4, 0,                   0, LT_INTEGER,  &NA_I4);
    STI(ST_INTEGER_I8,        "i8i", 8, 0,                   0, LT_INTEGER,  &NA_I8);
    STI(ST_REAL_F4,           "f4r", 4, 0,                   0, LT_REAL,     &NA_F4);
    STI(ST_REAL_F8,           "f8r", 8, 0,                   0, LT_REAL,     &NA_F8);
    STI(ST_REAL_I2,           "i2r", 2, sizeof(DecimalMeta), 0, LT_REAL,     &NA_I2);
    STI(ST_REAL_I4,           "i4r", 4, sizeof(DecimalMeta), 0, LT_REAL,     &NA_I4);
    STI(ST_REAL_I8,           "i8r", 8, sizeof(DecimalMeta), 0, LT_REAL,     &NA_I8);
    STI(ST_STRING_I4_VCHAR,   "i4s", 4, sizeof(VarcharMeta), 1, LT_STRING,   NULL);
    STI(ST_STRING_I8_VCHAR,   "i8s", 8, sizeof(VarcharMeta), 1, LT_STRING,   NULL);
    STI(ST_STRING_FCHAR,      "c#s", 0, sizeof(FixcharMeta), 0, LT_STRING,   NULL);
    STI(ST_STRING_U1_ENUM,    "u1e", 1, sizeof(EnumMeta),    1, LT_STRING,   &NA_U1);
    STI(ST_STRING_U2_ENUM,    "u2e", 2, sizeof(EnumMeta),    1, LT_STRING,   &NA_U2);
    STI(ST_STRING_U4_ENUM,    "u4e", 4, sizeof(EnumMeta),    1, LT_STRING,   &NA_U4);
    STI(ST_DATETIME_I8_EPOCH, "i8d", 8, 0,                   0, LT_DATETIME, &NA_I8);
    STI(ST_DATETIME_I8_PRTMN, "i8w", 8, 0,                   0, LT_DATETIME, &NA_I8);
    STI(ST_DATETIME_I4_TIME,  "i4t", 4, 0,                   0, LT_DATETIME, &NA_I4);
    STI(ST_DATETIME_I4_DATE,  "i4d", 4, 0,                   0, LT_DATETIME, &NA_I4);
    STI(ST_DATETIME_I2_MONTH, "i2d", 2, 0,                   0, LT_DATETIME, &NA_I2);
    STI(ST_OBJECT_PYPTR,      "p8p", 8, 0,                   0, LT_OBJECT,   NULL);
    #undef STI

    //---- More static asserts -------------------------------------------------
    // This checks validity of a cast used in the fread.c
    for (int i = -128; i <= 127; i++) {
        char ch = (char) i;
        int test1 = (ch >= '0' && ch <= '9');
        int test2 = ((uint_fast8_t)(ch - '0') < 10);
        assert(test1 == test2);
    }

    for (SType i = ST_VOID; i < DT_STYPES_COUNT; i++) {
        assert(i == stype_from_string(stype_info[i].code));
    }
}


/**
 * Helper function to convert SType from string representation into the numeric
 * constant. The string `s` must have length 3. If the stype is invalid, this
 * function will return ST_VOID.
 */
SType stype_from_string(const char *s)
{
    char s0 = s[0], s1 = s[1], s2 = s[2];
    if (s0 == 'i') {
        if (s2 == 'b') {
            if (s1 == '1') return ST_BOOLEAN_I1;
        } else if (s2 == 'i') {
            if (s1 == '1') return ST_INTEGER_I1;
            if (s1 == '2') return ST_INTEGER_I2;
            if (s1 == '4') return ST_INTEGER_I4;
            if (s1 == '8') return ST_INTEGER_I8;
        } else if (s2 == 'r') {
            if (s1 == '2') return ST_REAL_I2;
            if (s1 == '4') return ST_REAL_I4;
            if (s1 == '8') return ST_REAL_I8;
        } else if (s2 == 's') {
            if (s1 == '4') return ST_STRING_I4_VCHAR;
            if (s1 == '8') return ST_STRING_I8_VCHAR;
        } else if (s2 == 'd') {
            if (s1 == '2') return ST_DATETIME_I2_MONTH;
            if (s1 == '4') return ST_DATETIME_I4_DATE;
            if (s1 == '8') return ST_DATETIME_I8_EPOCH;
        } else if (s2 == 'w') {
            if (s1 == '8') return ST_DATETIME_I8_PRTMN;
        } else if (s2 == 't') {
            if (s1 == '4') return ST_DATETIME_I4_TIME;
        }
    } else if (s0 == 'f') {
        if (s2 == 'r') {
            if (s1 == '4') return ST_REAL_F4;
            if (s1 == '8') return ST_REAL_F8;
        }
    } else if (s0 == 'u') {
        if (s2 == 'e') {
            if (s1 == '1') return ST_STRING_U1_ENUM;
            if (s1 == '2') return ST_STRING_U2_ENUM;
            if (s1 == '4') return ST_STRING_U4_ENUM;
        }
    } else if (s0 == 'c') {
        if (s1 == '#' && s2 == 's') return ST_STRING_FCHAR;
    } else if (s0 == 'p') {
        if (s1 == '8' && s2 == 'p') return ST_OBJECT_PYPTR;
    }
    return ST_VOID;
}
