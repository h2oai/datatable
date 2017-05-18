#ifndef dt_TYPES_H
#define dt_TYPES_H
#include <stddef.h>  // size_t
#include <stdint.h>  // int*_t


// intXX(32)  =>  int32_t
// intXX(64)  =>  int64_t
// etc.
#define intXX(bits)  int ## bits ## _t



//==============================================================================

/**
 * "Logical" type of a data column.
 *
 * Logical type is supposed to match the user's notion of a column type. For
 * example logical "LT_INTEGER" type corresponds to the mathematical set of
 * integers, and thus reflects the usual notion of what the "integer" *is*.
 *
 * Each logical type has multiple underlying "storage" types, that describe
 * how the type is actually stored in memory. For example, LT_INTEGER can be
 * stored as an 8-, 16-, 32- or a 64-bit integer. All "storage" types within
 * a single logical type should be freely interchangeable: operators or
 * functions that accept certain logical type should be able to work with any
 * its storage subtype.
 *
 * Different logical types may or may not be interchangeable, depending on the
 * use case. For example, most binary operators would promote boolean ->
 * integer -> real; however some operators / functions may not. For example,
 * bit shift operators require integer (or boolean) arguments.
 *
 *
 * LT_MU
 *     special "marker" type for a column that has unknown type. For example,
 *     this can be used to indicate that the system should autodetect the
 *     column's type from the data. This type has no storage types.
 *
 * LT_BOOLEAN
 *     column for storing boolean (0/1) values. Right now we only allow to
 *     store booleans as 1-byte signed chars. In most arithmetic expressions
 *     booleans are automatically promoted to integers (or reals) if needed.
 *
 * LT_INTEGER
 *     integer values, equivalent of ℤ in mathematics. We support multiple
 *     storage sizes for integers: from 8 bits to 64 bits, but do not allow
 *     arbitrary-length integers. In most expressions integers will be
 *     automatically promoted to reals if needed.
 *
 * LT_REAL
 *     real values, equivalent of ℝ in mathematics. We store these in either
 *     fixed- or floating-point format.
 *
 * LT_STRING
 *     all strings are encoded in MUTF-8 (modified UTF-8), whose only
 *     distinction from the regular UTF-8 is that the null character is encoded
 *     as 0xC080 and not 0x00. In MUTF-8 null byte cannot appear, and is only
 *     used as an end-of-string marker.
 *
 * LT_DATETIME
 * LT_DURATION
 *
 * LT_OBJECT
 *     column for storing all other values of arbitrary (possibly heterogeneous)
 *     types. Each element is a `PyObject*`. Missing values are `Py_None`s.
 *
 */
typedef enum LType {
    LT_MU       = 0,
    LT_BOOLEAN  = 1,
    LT_INTEGER  = 2,
    LT_REAL     = 3,
    LT_STRING   = 4,
    LT_DATETIME = 5,
    LT_DURATION = 6,
    LT_OBJECT   = 7,
} __attribute__ ((__packed__)) LType;

#define DT_LTYPES_COUNT  (LT_OBJECT + 1)  // 1 + the largest LT_* type



//==============================================================================

/**
 * "Storage" type of a data column.
 *
 * These storage types are in 1-to-many correspondence with the logical types.
 * That is, a single logical type may have multiple storage types, but not the
 * other way around.
 *
 * ST_VOID
 *     "Fake" type, column with this stype is in an invalid state.
 *
 * -----------------------------------------------------------------------------
 *
 * ST_BOOLEAN_I1
 *     elem: signed char (1 byte)
 *     NA:   -128
 *     A boolean with True = 1, False = 0. All other values are invalid.
 *
 * -----------------------------------------------------------------------------
 *
 * ST_INTEGER_I1
 *     elem: signed char (1 byte)
 *     NA:   -2**7 = -128
 *     An integer in the range -127 .. 127.
 *
 * ST_INTEGER_I2
 *     elem: short int (2 bytes)
 *     NA:   -2**15 = -32768
 *     An integer in the range -32767 .. 32767.
 *
 * ST_INTEGER_I4
 *     elem: int (4 bytes)
 *     NA:   -2**31 = -2147483648
 *     An integer in the range -2147483647 .. 2147483647.
 *
 * ST_INTEGER_I8
 *     elem: long int (8 bytes)
 *     NA:   -2**63 = -9223372036854775808
 *     An integer in the range -9223372036854775807 .. 9223372036854775807.
 *
 * -----------------------------------------------------------------------------
 *
 * ST_REAL_F4
 *     elem: float (4 bytes)
 *     NA:   0x7F8007A2
 *     Floating-point real number, corresponding to C's `float` (IEEE 754). We
 *     designate a specific NAN payload to mean the NA value; whereas all other
 *     numbers starting with 0x7F8 or 0xFF8 should be treated as actual NANs (or
 *     infinities).
 *
 * ST_REAL_F8
 *     elem: double (8 bytes)
 *     NA:   0x7FF00000000007A2
 *     Floating-point real number, corresponding to C's `double` (IEEE 754).
 *
 * ST_REAL_I2
 *     elem: short int (2 bytes)
 *     NA:   -2**15 = -32768
 *     meta: `scale` (int)
 *     The fixed-point real number (aka decimal); the scale variable in the meta
 *     indicates the number of digits after the decimal point. For example,
 *     number 7.11 can be stored as integer 711 with scale = 2.
 *     Note that this is different from IEEE 754 "decimal" format, since we
 *     include scale into the meta information of the column, rather than into
 *     each value. Thus, all values will have common scale, which greatly
 *     simplifies their use.
 *
 * ST_REAL_I4
 *     elem: int (4 bytes)
 *     NA:   -2**31
 *     meta: `scale` (int)
 *     Fixed-point real number stored as a 32-bit integer.
 *
 * ST_REAL_I8
 *     elem: long int (8 bytes)
 *     NA:   -2**63
 *     meta: `scale` (int)
 *     Fixed-point real number stored as a 64-bit integer.
 *
 * -----------------------------------------------------------------------------
 *
 * ST_STRING_I4_VCHAR
 *     elem: int (4 bytes) + char[]
 *     NA:   negative numbers
 *     meta: `offoff` (long int)
 *     Variable-width strings. The data buffer has the following structure:
 *     First comes a section with string data: all non-NA strings are UTF-8
 *     encoded and placed end-to-end. This section is padded by 0xFF bytes so
 *     that it has a length which is a multiple of 8. After that comes the
 *     array of int32_t primitives representing offsets of each string in the
 *     buffer. In particular, each entry is the offset of the last byte of the
 *     string within the data buffer. NA strings are encoded as negation of the
 *     previous string's offset.
 *     Thus, i'th string is NA if its offset is negative, otherwise its a valid
 *     string whose starting offset is `start(i) = i? abs(off(i-1)) - 1 : 0`,
 *     ending offset is `end(i) = off(i) - 1`, and `len(i) = end(i) - start(i)`.
 *     For example, a column with 4 values `[N/A, "hello", "", N/A]` will be
 *     encoded as a buffer of size 24 = (5 + 0) + (3) + (4 * 4):
 *         h e l l o 0xFF 0xFF 0xFF <-1> <6> <6> <-6>
 *         meta = 8
 *     (where "<n>" denotes the 4-byte sequence encoding integer `n`).
 *     Meta information stores the offset of the section with offsets. Thus the
 *     total buffer size is always `offoff` + 4 * `nrows`.
 *     Note: 0xFF is used for padding because it's not a valid UTF-8 byte.
 *
 * ST_STRING_I8_VCHAR
 *     elem: long int (8 bytes) + char[]
 *     NA:   negative numbers
 *     meta: `offoff` (long int)
 *     Variable-width strings: same as ST_STRING_I4_VCHAR but use 64-bit
 *     offsets.
 *
 * ST_STRING_FCHAR
 *     elem: char[] (n bytes)
 *     NA:   0xFF 0xFF ... 0xFF
 *     meta: `n` (int)
 *     Fixed-width strings, similar to "CHAR(n)" in SQL. These strings have
 *     constant width `n` and are therefore stored as `char[n]` arrays. They are
 *     *not* null-terminated, however strings that are shorter than `n` in width
 *     will be \0-padded. The width `n` > 0 is given in the metadata. String
 *     data is encoded in UTF-8. The NA value is encoded as a sequence of 0xFF
 *     bytes, which is not a valid UTF-8 string.
 *
 * ST_STRING_U1_ENUM
 *     elem: unsigned char (1 byte)
 *     NA:   255
 *     meta: `offoff` (long int), `dataoff` (long int), `nlevels` (int)
 *     String column stored as a categorical variable (aka "factor" or "enum").
 *     This type is suitable for columns with low cardinality, i.e. having no
 *     more than 255 distinct string values.
 *     The data is stored in a buffer with 3 regions: first comes the string
 *     data, padded to a multiple of 8 bytes; then comes the array of int32
 *     offsets within the data buffer, these offsets describe the boundaries of
 *     individual strings within the data section (this array is also padded
 *     to a multiple of 8 bytes); finally comes the array of categorical
 *     indices. Meta information contains offsets of the second and the third
 *     sections. The layout of the first 2 sections is exactly the same as
 *     that of the ST_STRING_I4_VCHAR type.
 *
 * ST_STRING_U2_ENUM
 *     elem: unsigned short int (2 bytes)
 *     NA:   65535
 *     meta: `offoff` (long int), `dataoff` (long int), `nlevels` (int)
 *     Strings stored as a categorical variable with no more than 65535 distinct
 *     levels. The layout is exactly the same as that of ST_STRING_U1_ENUM, only
 *     the last section uses 2 bytes per element instead of just 1 byte.
 *
 * ST_STRING_U4_ENUM
 *     elem: unsigned int (4 bytes)
 *     NA:   2**32-1
 *     meta: `offoff` (long int), `dataoff` (long int), `nlevels` (int)
 *     Strings stored as a categorical variable with no more than 2**32-1
 *     distinct levels. (The combined size of all categorical strings may not
 *     exceed 2**32 too). The layout is same as that of ST_STRING_U1_ENUM, only
 *     the last section uses 4 bytes per element instead of just 1 byte.
 *
 *
 * -----------------------------------------------------------------------------
 *
 * ST_DATETIME_I8_EPOCH
 *     elem: long int (8 bytes)
 *     NA:   -2**63
 *     Timestamp, stored as the number of microseconds since 0000-03-01. The
 *     allowed time range is ≈290,000 years around the epoch. The time is
 *     assumed to be in UTC, and does not allow specifying a time zone.
 *
 * ST_DATETIME_I8_PRTMN
 *     elem: long int (8 bytes)
 *     NA:   -2**63
 *     Timestamp, stored as YYYYMMDDhhmmssmmmuuu, i.e. concatenated date parts.
 *     The widths of each subfield are:
 *         YYYY: years,        18 bits (signed)
 *           MM: months,        4 bits
 *           DD: days,          5 bits
 *           hh: hours,         5 bits
 *           mm: minutes,       6 bits
 *           ss: seconds,       6 bits
 *          mmm: milliseconds, 10 bits
 *          uuu: microseconds, 10 bits
 *     The allowed time range is ≈131,000 years around the epoch. The time is
 *     in UTC, and does not allow specifying a time zone.
 *
 * ST_DATETIME_I4_TIME
 *     elem: int (4 bytes)
 *     NA:   -2**31
 *     Time only: the number of milliseconds since midnight. The allowed time
 *     range is ≈24 days.
 *
 * ST_DATETIME_I4_DATE
 *     elem: int (4 bytes)
 *     NA:   -2**31
 *     Date only: the number of days since 0000-03-01. The allowed time range
 *     is ≈245,000 years.
 *
 * ST_DATETIME_I2_MONTH
 *     elem: short int (2 bytes)
 *     NA:   -2**15
 *     Year+month only: the number of months since 0000-03-01. The allowed time
 *     range is up to year 2730.
 *     This type is specifically designed for business applications. It allows
 *     adding/subtraction in monthly/yearly intervals (other datetime types do
 *     not allow that since months/years have uneven lengths).
 *
 *
 * -----------------------------------------------------------------------------
 *
 * ST_OBJECT_PTR
 *     elem: PyObject*
 *     NA:   &Py_None
 *
 */
typedef enum SType {
    ST_VOID              = 0,
    ST_BOOLEAN_I1        = 1,
    ST_INTEGER_I1        = 2,
    ST_INTEGER_I2        = 3,
    ST_INTEGER_I4        = 4,
    ST_INTEGER_I8        = 5,
    ST_REAL_F4           = 6,
    ST_REAL_F8           = 7,
    ST_REAL_I2           = 8,
    ST_REAL_I4           = 9,
    ST_REAL_I8           = 10,
    ST_STRING_I4_VCHAR   = 11,
    ST_STRING_I8_VCHAR   = 12,
    ST_STRING_FCHAR      = 13,
    ST_STRING_U1_ENUM    = 14,
    ST_STRING_U2_ENUM    = 15,
    ST_STRING_U4_ENUM    = 16,
    ST_DATETIME_I8_EPOCH = 17,
    ST_DATETIME_I8_PRTMN = 18,
    ST_DATETIME_I4_TIME  = 19,
    ST_DATETIME_I4_DATE  = 20,
    ST_DATETIME_I2_MONTH = 21,
    ST_OBJECT_PYPTR      = 22,
} __attribute__ ((__packed__)) SType;

#define DT_STYPES_COUNT  (ST_OBJECT_PYPTR + 1)



//==============================================================================

/**
 * Information about `SType`s, for programmatic access.
 *
 * code:
 *     0-terminated 3-character string representing the stype in a form easily
 *     understandable by humans.
 *
 * elemsize:
 *     number of storage bytes per element (for fixed-size types), so that the
 *     amount of memory required to store a column with `n` rows would be
 *     `n * elemsize`. For variable-size types, this field gives the minimal
 *     storage size per element.
 *
 * metasize:
 *     size of the meta structure associated with the field. If the field
 *     doesn't need meta then this will be zero.
 *
 * varwidth:
 *     flag indicating whether the field is variable-width. If this is false,
 *     then the column is a plain array of elements, each of `elemsize` bytes
 *     (except for ST_STRING_FCHAR, where each element's size is `meta->n`).
 *     If this flag is true, then the field has more complex layout and
 *     specialized logic to handle that layout.
 *
 * ltype:
 *     which :enum:`LType` corresponds to this SType.
 *
 */
typedef struct STypeInfo {
    char    code[4];
    size_t  elemsize;
    size_t  metasize;
    _Bool   varwidth;
    LType   ltype;
} STypeInfo;

extern STypeInfo stype_info[DT_STYPES_COUNT];



//==============================================================================

/**
 * Structs for meta information associated with particular types.
 *
 * DecimalMeta (ST_REAL_I2, ST_REAL_I4, ST_REAL_I8)
 *     scale: the number of digits after the decimal point. For example stored
 *            value 123 represents actual value 1.23 when `scale = 2`, or
 *            actual value 123000 when `scale = -3`.
 *
 * VarcharMeta (ST_STRING_I4_VCHAR, ST_STRING_I8_VCHAR)
 *     offoff: location within the data buffer of the section with offsets.
 *
 * FixcharMeta (ST_STRING_FCHAR)
 *     n: number of characters in the fixed-width string.
 *
 * EnumMeta (ST_STRING_U1_ENUM, ST_STRING_U2_ENUM, ST_STRING_U4_ENUM)
 *     offoff: location within the data buffer of the section with offsets.
 *     dataoff: location of the section with categorical levels (one per row).
 *     nlevels: total number of categorical levels.
 *
 */
typedef struct DecimalMeta {  // ST_REAL_IX
    int32_t scale;
} DecimalMeta;

typedef struct VarcharMeta {  // ST_STRING_IX_VCHAR
    int64_t offoff;
} VarcharMeta;

typedef struct FixcharMeta {  // ST_STRING_FCHAR
    int32_t n;
} FixcharMeta;

typedef struct EnumMeta {     // ST_STRING_UX_ENUM
    int64_t offoff;
    int64_t dataoff;
    int32_t nlevels;
} EnumMeta;



//==============================================================================

/**
 * NA constants
 *
 * Integer-based NAs can be compared by value (e.g. `x == NA_I4`), whereas
 * floating-point NAs require special functions `ISNA_F4(x)` and `ISNA_F8(x)`.
 */

extern const int8_t   NA_I1;
extern const int16_t  NA_I2;
extern const int32_t  NA_I4;
extern const int64_t  NA_I8;
extern const uint8_t  NA_U1;
extern const uint16_t NA_U2;
extern const uint32_t NA_U4;
extern const uint64_t NA_U8;
extern       float    NA_F4;
extern       double   NA_F8;

inline int ISNA_F4(float x);
inline int ISNA_F8(double x);
inline int ISNA_I1(int8_t x);
inline int ISNA_I2(int16_t x);
inline int ISNA_I4(int32_t x);
inline int ISNA_I8(int64_t x);
inline int ISNA_U1(uint8_t x);
inline int ISNA_U2(uint16_t x);
inline int ISNA_U4(uint32_t x);



//==============================================================================

// Initializer function
void init_types(void);



#endif
