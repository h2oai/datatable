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
#ifndef dt_FRAME_PYFRAME_h
#define dt_FRAME_PYFRAME_h
#include "python/xobject.h"
#include "datatable.h"


namespace py {


/**
 * This class currently serves as a base for datatable.Frame, but eventually
 * all functionality will be moved here, and this class will be the main
 * user-facing Frame class.
 */
class Frame : public XObject<Frame> {
  private:
    DataTable* dt;  // owned (cannot use unique_ptr because this class's
                    // destructor is never called by Python)
    mutable PyObject* stypes;  // memoized tuple of stypes
    mutable PyObject* ltypes;  // memoized tuple of ltypes

  public:
    static void impl_init_type(XTypeMaker&);
    static void _init_cbind(XTypeMaker&);
    static void _init_init(XTypeMaker&);
    static void _init_iter(XTypeMaker&);
    static void _init_jay(XTypeMaker&);
    static void _init_key(XTypeMaker&);
    static void _init_names(XTypeMaker&);
    static void _init_rbind(XTypeMaker&);
    static void _init_replace(XTypeMaker&);
    static void _init_repr(XTypeMaker&);
    static void _init_sizeof(XTypeMaker&);
    static void _init_sort(XTypeMaker&);
    static void _init_stats(XTypeMaker&);
    static void _init_tocsv(XTypeMaker&);
    static void _init_tonumpy(XTypeMaker&);
    static void _init_topython(XTypeMaker&);

    // Internal "constructor" of Frame objects. We do not use real constructors
    // because Frame objects must be allocated/initialized by Python.
    static oobj oframe(DataTable* dt);
    static oobj oframe(DataTable&& dt);

    // Convert python object `src` into a py::Frame object. This is exactly
    // equivalent to calling `dt.Frame(src)` in python.
    static oobj oframe(robj src);

    DataTable* get_datatable() const { return dt; }

    void m__init__(const PKArgs&);
    void m__dealloc__();
    oobj m__getitem__(robj item);
    void m__setitem__(robj item, robj value);
    oobj m__getstate__(const PKArgs&);  // pickling support
    void m__setstate__(const PKArgs&);
    oobj m__sizeof__(const PKArgs&);
    void m__getbuffer__(Py_buffer* view, int flags);
    void m__releasebuffer__(Py_buffer* view);
    oobj m__iter__();
    oobj m__reversed__();
    oobj m__copy__();
    oobj m__deepcopy__(const PKArgs&);

    // Frame display
    oobj m__repr__() const;
    oobj m__str__() const;
    oobj _repr_html_(const PKArgs&);
    oobj _repr_pretty_(const PKArgs&);
    void view(const PKArgs&);
    oobj newview(const PKArgs&);

    oobj get_ncols() const;
    oobj get_nrows() const;
    oobj get_ndims() const;
    oobj get_shape() const;
    oobj get_stypes() const;
    oobj get_stype() const;
    oobj get_ltypes() const;
    oobj get_names() const;
    oobj get_key() const;
    void set_nrows(const Arg&);
    void set_names(const Arg&);
    void set_key(const Arg&);
    void set_internal(robj);

    void cbind(const PKArgs&);
    oobj colindex(const PKArgs&);
    oobj copy(const PKArgs&);
    oobj head(const PKArgs&);
    void materialize(const PKArgs&);
    void rbind(const PKArgs&);
    void repeat(const PKArgs&);
    void replace(const PKArgs&);
    oobj sort(const PKArgs&);
    oobj tail(const PKArgs&);
    oobj export_names(const PKArgs&);

    // Conversion methods
    oobj to_csv(const PKArgs&);
    oobj to_dict(const PKArgs&);
    oobj to_jay(const PKArgs&);  // See jay/save_jay.cc
    oobj to_list(const PKArgs&);
    oobj to_numpy(const PKArgs&);
    oobj to_pandas(const PKArgs&);
    oobj to_tuples(const PKArgs&);

    // Stats functions
    oobj stat(const PKArgs&);
    oobj stat1(const PKArgs&);

    // Exposed to users as `dt.frame_integrity_check(frame)` function
    void integrity_check();

    // Called once during module start-up
    static void init_names_options();
    static void init_display_options();

  private:
    static bool internal_construction;
    class NameProvider;

    ~Frame() {}
    void _clear_types() const;
    void _init_names() const;
    void _init_inames() const;
    void _fill_default_names();
    void _dedup_and_save_names(NameProvider*);
    void _replace_names_from_map(py::odict);

    // getitem / setitem support
    oobj _main_getset(robj item, robj value);
    oobj _get_single_column(robj selector);
    oobj _del_single_column(robj selector);

    friend class FrameInitializationManager;
    friend class pylistNP;
    friend class strvecNP;
};

extern PyObject* Frame_Type;

}  // namespace py

#endif
