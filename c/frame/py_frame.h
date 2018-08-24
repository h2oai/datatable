//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_FRAME_PYFRAME_h
#define dt_FRAME_PYFRAME_h
#include "python/ext_type.h"
#include "datatable.h"
#include "py_datatable.h"

namespace pydatatable {  // temp
  void _clear_types(obj*);
  PyObject* check(obj*, PyObject*);
}


namespace py {


/**
 * This class currently serves as a base for datatable.Frame, but eventually
 * all functionality will be moved here, and this class will be the main
 * user-facing Frame class.
 */
class Frame : public PyObject {
  private:
    DataTable* dt;
    mutable PyObject* stypes;  // memoized tuple of stypes
    mutable PyObject* ltypes;  // memoized tuple of ltypes
    mutable PyObject* names;   // memoized tuple of column names
    mutable PyObject* inames;  // memoized dict of {column name: index}

    pydatatable::obj* core_dt;  // TODO: remove

  public:
    class Type : public ExtType<Frame> {
      public:
        static PKArgs args___init__;
        static PKArgs args_colindex;
        static const char* classname();
        static const char* classdoc();
        static bool is_subclassable() { return true; }

        static void init_getsetters(GetSetters& gs);
        static void init_methods(Methods& gs);
    };

    void m__init__(PKArgs&);
    void m__dealloc__();
    void m__get_buffer__(Py_buffer* buf, int flags) const;
    void m__release_buffer__(Py_buffer* buf) const;

    oobj get_ncols() const;
    oobj get_nrows() const;
    oobj get_shape() const;
    oobj get_stypes() const;
    oobj get_ltypes() const;
    oobj get_names() const;
    oobj get_key() const;
    oobj get_internal() const;
    void set_nrows(obj);
    void set_names(obj);
    void set_internal(obj);

    oobj colindex(PKArgs&);

  private:
    void _clear_names();
    void _init_names() const;
    void _init_inames() const;
    void _fill_default_names();
    void _dedup_and_save_names(py::olist);
    void _replace_names_from_map(py::odict);
    Error _name_not_found_error(const std::string& name);

    friend void pydatatable::_clear_types(pydatatable::obj*); // temp
    friend PyObject* pydatatable::check(pydatatable::obj*, PyObject*); // temp
};



}  // namespace py

#endif
