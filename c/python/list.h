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
#include "python/obj.h"

class PyyList;
// class PyyLong;
// class PyyFloat;
namespace py { class list; class Float; }


class PyyListEntry {
  private:
    PyObject* list;  // borrowed ref
    Py_ssize_t i;

  public:
    PyyListEntry(PyObject* list, Py_ssize_t i);

    operator py::oobj() const;
    operator py::obj() const;
    operator PyyList() const;
    // operator PyyLong() const;
    operator py::Float() const;
    PyyListEntry& operator=(PyObject*);
    PyyListEntry& operator=(py::oobj&&);

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
 *   as a reference: it can be both read (by casting into `obj`) and written
 *   by assigning a `PyObject*` / `oobj` to it.
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
    friend py::list;
};



namespace py {
class Arg;


class list : public oobj {
  private:
    bool is_list;
    size_t : 56;

  public:
    using oobj::oobj;
    list();
    list(PyObject*);
    list(const PyyList&);  // temp
    PyyList to_pyylist() const;

    operator bool() const;
    size_t size() const;
    obj operator[](size_t i) const;

    friend class Arg;
};


}  // namespace py

#endif
