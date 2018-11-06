//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
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

    pydatatable::obj* core_dt;  // TODO: remove

  public:
    class Type : public ExtType<Frame> {
      public:
        static PKArgs args___init__;
        static NoArgs args__repr_html_;
        static PKArgs args_cbind;
        static PKArgs args_colindex;
        static PKArgs args_replace;
        static NoArgs args_copy;
        static const char* classname();
        static const char* classdoc();
        static bool is_subclassable() { return true; }
        static void init_methods_and_getsets(Methods&, GetSetters&);
      private:
        static void _init_names(Methods&, GetSetters&);
    };

    // Internal "constructor" of Frame objects. We do not use real constructors
    // because Frame objects must be allocated/initialized by Python.
    static Frame* from_datatable(DataTable* dt);
    DataTable* get_datatable() const { return dt; }

    void m__init__(PKArgs&);
    void m__dealloc__();
    void m__get_buffer__(Py_buffer* buf, int flags) const;
    void m__release_buffer__(Py_buffer* buf) const;
    oobj m__getitem__(obj item);
    void m__setitem__(obj item, obj value);

    oobj _repr_html_(const NoArgs&);
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
    void set_keys(obj);
    void set_internal(obj);

    void cbind(const PKArgs&);
    oobj colindex(const PKArgs&);
    oobj copy(const NoArgs&);
    void replace(const PKArgs&);

  private:
    static bool internal_construction;
    class NameProvider;
    void _clear_types() const;
    void _clear_names();
    void _init_names() const;
    void _init_inames() const;
    void _fill_default_names();
    void _dedup_and_save_names(NameProvider*);
    void _replace_names_from_map(py::odict);
    Error _name_not_found_error(const std::string& name);

    // getitem / setitem support
    oobj _fast_getset(obj item, obj value);
    oobj _main_getset(obj item, obj value);
    oobj _fallback_getset(obj item, obj value);

    friend void pydatatable::_clear_types(pydatatable::obj*); // temp
    friend PyObject* pydatatable::check(pydatatable::obj*, PyObject*); // temp
    friend class FrameInitializationManager;
    friend class pylistNP;
    friend class strvecNP;
};

extern PyObject* Frame_Type;
extern PyObject* fread_fn;
extern PyObject* fallback_makedatatable;

}  // namespace py

#endif
