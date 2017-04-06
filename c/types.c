#include "types.h"

size_t ColType_size[DT_LTYPE_COUNT];

int STypeToLType[DT_STYPES_COUNT + 1];

int init_types(void) {
    STypeToLType[DT_VOID] = -1;
    STypeToLType[DT_BOOLEAN_I8] = DT_BOOLEAN;
    STypeToLType[DT_INTEGER_I8]  = DT_INTEGER;
    STypeToLType[DT_INTEGER_I16] = DT_INTEGER;
    STypeToLType[DT_INTEGER_I32] = DT_INTEGER;
    STypeToLType[DT_INTEGER_I64] = DT_INTEGER;
    STypeToLType[DT_REAL_F32] = DT_REAL;
    STypeToLType[DT_REAL_F64] = DT_REAL;
    STypeToLType[DT_REAL_I16] = DT_REAL;
    STypeToLType[DT_REAL_I32] = DT_REAL;
    STypeToLType[DT_REAL_I64] = DT_REAL;
    STypeToLType[DT_STRING_UI32_VCHAR] = DT_STRING;
    STypeToLType[DT_STRING_UI64_VCHAR] = DT_STRING;
    STypeToLType[DT_STRING_FCHAR]      = DT_STRING;
    STypeToLType[DT_STRING_UI8_ENUM]   = DT_STRING;
    STypeToLType[DT_STRING_UI16_ENUM]  = DT_STRING;
    STypeToLType[DT_STRING_UI32_ENUM]  = DT_STRING;
    STypeToLType[DT_DATETIME_I64_EPOCH] = DT_DATETIME;
    STypeToLType[DT_DATETIME_I64_PRTMN] = DT_DATETIME;
    STypeToLType[DT_DATETIME_I32_TIME]  = DT_DATETIME;
    STypeToLType[DT_DATETIME_I32_DATE]  = DT_DATETIME;
    STypeToLType[DT_DATETIME_I16_MONTH] = DT_DATETIME;
    STypeToLType[DT_STYPES_COUNT] = 0;

    ColType_size[DT_MU] = 0;
    ColType_size[DT_REAL] = sizeof(double);
    ColType_size[DT_INTEGER] = sizeof(long);
    ColType_size[DT_BOOLEAN] = sizeof(char);
    ColType_size[DT_STRING] = sizeof(char*);
    ColType_size[DT_OBJECT] = sizeof(void*);

    return 1;
}
