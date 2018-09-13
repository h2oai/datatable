//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_LIST_h
#define dt_PYTHON_LIST_h
#include <Python.h>
#include "utils/pyobj.h"

class PyyList;
class PyyLong;
class PyyFloat;


class PyyListEntry {
  private:
    PyObject* list;  // borrowed ref
    Py_ssize_t i;

  public:
    PyyListEntry(PyObject* list, Py_ssize_t i);

    operator PyObj() const;
    operator PyyList() const;
    operator PyyLong() const;
    operator PyyFloat() const;
    PyyListEntry& operator=(PyObject*);
    PyyListEntry& operator=(const PyObj&);

    PyObject* as_new_ref() const;

  private:
    PyObject* get() const;  // returns a borrowed ref
};



/**
 * PyyList: C++ wrapper around pythonic PyList class. The wrapper holds a
 * persistent reference to the underlying `list` (in the sense that it INCREFs
 * it upon acquiral and decrefs during destruction).
 *
 * Construction
 * ------------
 * PyyList()
 *   Construct a void PyyList object (not backed by any PyObject*).
 *
 * PyyList(size_t n)
 *   Creates a new Pythonic list of size `n` and returns it as PyyList object.
 *   Initially the list is filled with NULLs, the user must fill it with actual
 *   values before returning to Python.
 *
 * PyyList(PyObject* src)
 *   Construct PyyList from the provided PyObject (which must be an instance of
 *   PyList_Object, or Py_None). If `src` is NULL or not a PyList, an exception
 *   will be raised. If `src` was a "newref", the caller is still responsible
 *   for decrefing it.
 *
 * PyyList(const PyyList&)
 * PyyList(PyyList&&)
 *   Copy- and move- constructors.
 *
 *
 * Public API
 * ----------
 * size()
 *   Returns the number of elements in the list.
 *
 * operator bool()
 *   Returns true if the object is non-void (i.e. wraps a non-NULL PyObject).
 *   Note that this will return true even if the underlying list has 0 size.
 *
 * operator[](i)
 *   Returns i-th element of the list as a PyyListEntry object. This entry acts
 *   as a reference: it can be both read (by casting into `PyObj`) and written
 *   by assigning a `PyObject*` / `PyObj` to it.
 *
 * release()
 *   Converts PyyList into the primitive `PyObject*` which can be used by
 *   Python. This method is destructive: the PyyList object becomes "void" after
 *   this method is called. Note that the returned value is a "new reference":
 *   the caller is responsible for dec-refing it eventually.
 *
 */
class PyyList {
  private:
    PyObject* list;

  public:
    PyyList();
    PyyList(size_t n);
    PyyList(PyObject*);
    PyyList(const PyyList&);
    PyyList(PyyList&&);
    ~PyyList();

    size_t size() const noexcept;
    operator bool() const noexcept;
    PyyListEntry operator[](size_t i) const;
    PyObject* release();

    friend void swap(PyyList& first, PyyList& second) noexcept;
};



namespace py {
class Arg;


class List {
  private:
    PyObject* pyobj;  // owned ref

  public:
    List(const List&);
    List(List&&);
    ~List();

    size_t size() const;

  private:
    List(PyObject* o);
    friend class Arg;
};


}  // namespace py

#endif
