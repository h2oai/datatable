#ifndef dt_COLTYPE_H
#define dt_COLTYPE_H


/**
 * Type for a column.
 *
 * DT_AUTO
 *     special "marker" type to indicate that the system should autodetect the
 *     column's type from the data. This value must not be used in an actual
 *     DataTable instance.
 *
 * DT_DOUBLE
 *     column for storing floating-point values. Each element is a `double`.
 *     Missing values are represented natively as `NA` values.
 *
 * DT_LONG
 *     column for storing integer values. Each element is a 64-bit integer.
 *     Missing values are represented as the `LONG_MIN` constant.
 *
 * DT_BOOL
 *     column for storing boolean (0/1) values. Each element is a `char`
 *     (1-byte integer). Value of 0 is considered False, 1 is True, and all
 *     other values represent missing values (usually stored as value 2).
 *
 * DT_STRING
 *     (not implemented)
 *
 * DT_OBJECT
 *     column for storing all other values of arbitrary (possibly heterogeneous)
 *     types. Each element is a `PyObject*`. Missing values are `Py_None`s.
 *
 */
typedef enum ColType {
    DT_AUTO    = 0,
    DT_DOUBLE  = 1,
    DT_LONG    = 2,
    DT_STRING  = 3,
    DT_BOOL    = 4,
    DT_OBJECT  = 5
} ColType;

#define DT_COUNT DT_OBJECT + 1  // 1 more than the largest DT_* type

extern int ColType_size[DT_COUNT];


#endif
