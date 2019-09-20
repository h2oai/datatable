//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#ifndef dt_API_h
#define dt_API_h
#include <locale>  // do not remove (#1824)
#include <Python.h>

// This header file may be included from C code
#ifdef __cplusplus
extern "C" {
#endif

#define DtStype_BOOL     1
#define DtStype_INT8     2
#define DtStype_INT16    3
#define DtStype_INT32    4
#define DtStype_INT64    5
#define DtStype_FLOAT32  6
#define DtStype_FLOAT64  7
#define DtStype_STR32    11
#define DtStype_STR64    12
#define DtStype_OBJ      21

/**
 * Return the ABI version of the currently linked datatable library. The ABI
 * version will be increased every time new functions are added to this header
 * file, or some of the existing functions change parameters or behavior.
 */
size_t DtABIVersion();



//-------- Frame ---------------------------------------------------------------

/**
 * Return 1 if `ob` is a datatable.Frame object, and 0 otherwise.
 */
int DtFrame_Check(PyObject* ob);


/**
 * Return the number of rows/columns in a Frame. The object `pydt` must be
 * a `datatable.Frame`.
 */
size_t DtFrame_NRows(PyObject* pydt);
size_t DtFrame_NColumns(PyObject* pydt);


/**
 * Return SType of the `i`-th column in Frame `pydt`. If the column does not
 * exist, returns -1.
 */
int DtFrame_ColumnStype(PyObject* pydt, size_t i);


/**
 * Return the boolean indicator of whether the column `i` in Frame `pydt` is
 * virtual or not. If the column `i` does not exist, sets an error and returns
 * -1.
 */
int DtFrame_ColumnIsVirtual(PyObject* pydt, size_t i);


/**
 * Return a pointer to the internal data buffer of column `i` in Frame `pydt`.
 *
 * Use the first method if you're only interested in reading the data from the
 * buffer, or the second method if you want to be able to both read and write
 * the data. Note that writing into the readonly buffer may cause errors or in
 * rare cases crashes. At the same time, requesting a writable buffer will
 * cause the data to be copied in case it is shared between several columns.
 *
 * The pointer returned is considered a borrowed reference: do not try to
 * free it. The pointer may be invalidated during subsequent calls to datatable,
 * so it is not recommended to store the pointer for a long period of time.
 *
 * The pointer is returned as `void*`, however it should really be interpreted
 * as `int64_t[]`, `int32_t[]`, `double[]`, ..., depending on the stype of the
 * column (see :function:`DtFrame_ColumnStype`). For string columns this method
 * returns the "offsets" part of the data.
 *
 * If an error occurs, a null pointer is returned.
 */
const void* DtFrame_ColumnDataR(PyObject* pydt, size_t i);
void* DtFrame_ColumnDataW(PyObject* pydt, size_t i);


/**
 * Return a pointer to the string data buffer of a column `i` in Frame `pydt`.
 * If the column is not of string type (STR32 or STR64), a null pointer will
 * be returned and an error set.
 *
 * The returned pointer is a borrowed reference. It may be invalidated during
 * the subsequent calls to datatable.
 */
const char* DtFrame_ColumnStringDataR(PyObject* pydt, size_t i);





#ifdef __cplusplus
}
#endif

#endif
