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
#include "frame/py_frame.h"
#include <iostream>
#include <string>
#include <vector>
#include "python/_all.h"
#include "python/list.h"
#include "python/oiter.h"
#include "python/orange.h"
#include "python/oset.h"
#include "python/string.h"
#include "utils/alloc.h"
#include "ztest.h"

namespace py {


//------------------------------------------------------------------------------
// Frame construction manager
//------------------------------------------------------------------------------

class FrameInitializationManager {
  private:
    PKArgs& all_args;
    const Arg& src;
    const Arg& names_arg;
    const Arg& stypes_arg;
    const Arg& stype_arg;
    bool defined_names;
    bool defined_stypes;
    bool defined_stype;
    SType stype0;
    int : 32;
    Frame* frame;
    std::vector<Column*> cols;

    class em : public py::_obj::error_manager {
      Error error_not_stype(PyObject*) const override;
    };


  //----------------------------------------------------------------------------
  // External API
  //----------------------------------------------------------------------------
  public:
    FrameInitializationManager(PKArgs& args, Frame* f)
      : all_args(args),
        src(args[0]),
        names_arg(args[1]),
        stypes_arg(args[2]),
        stype_arg(args[3]),
        frame(f)
    {
      defined_names  = !(names_arg.is_undefined() || names_arg.is_none());
      defined_stypes = !(stypes_arg.is_undefined() || stypes_arg.is_none());
      defined_stype  = !(stype_arg.is_undefined() || stype_arg.is_none());
      if (defined_stype && defined_stypes) {
        throw TypeError() << "You can pass either parameter `stypes` or "
            "`stype` to Frame() constructor, but not both at the same time";
      }
      if (defined_stype) {
        stype0 = stype_arg.to_stype(em());
      }
      if (src && all_args.num_varkwd_args() > 0) {
        throw _error_unknown_kwargs();
      }
    }

    ~FrameInitializationManager() {
      for (auto col : cols) dt::free(col);
    }


    void run()
    {
      if (src.is_list_or_tuple()) {
        py::olist collist = src.to_pylist();
        if (collist.size() == 0) {
          return init_empty_frame();
        }
        py::robj item0 = collist[0];
        if (item0.is_list() || item0.is_range() || item0.is_buffer()) {
          return init_from_list_of_lists();
        }
        if (item0.is_dict()) {
          if (names_arg)
            return init_from_list_of_dicts_fixed_keys();
          else
            return init_from_list_of_dicts_auto_keys();
        }
        if (item0.is_tuple()) {
          return init_from_list_of_tuples();
        }
        return init_from_list_of_primitives();
      }
      if (src.is_dict()) {
        return init_from_dict();
      }
      if (src.is_range()) {
        return init_from_list_of_primitives();
      }
      if (all_args.num_varkwd_args()) {
        // Already checked in the constructor that `src` is undefined.
        return init_from_varkwds();
      }
      if (src.is_frame()) {
        return init_from_frame();
      }
      if (src.is_string()) {
        return init_from_string();
      }
      if (src.is_undefined() || src.is_none()) {
        return init_empty_frame();
      }
      if (src.is_pandas_frame() || src.is_pandas_series()) {
        return init_from_pandas();
      }
      if (src.is_numpy_array()) {
        return init_from_numpy();
      }
      if (src.is_ellipsis() &&
               !defined_names && !defined_stypes && !defined_stype) {
        return init_mystery_frame();
      }
      throw TypeError() << "Cannot create Frame from " << src.typeobj();
    }



  //----------------------------------------------------------------------------
  // Frame creation methods
  //----------------------------------------------------------------------------
  private:
    void init_empty_frame() {
      if (defined_names) {
        if (!names_arg.is_list_or_tuple()) check_names_count(0);
        size_t ncols = names_arg.to_pylist().size();
        check_stypes_count(ncols);
        py::olist empty_list(0);
        for (size_t i = 0; i < ncols; ++i) {
          SType s = get_stype_for_column(i);
          make_column(empty_list, s);
        }
        make_datatable(names_arg);
      } else {
        check_stypes_count(0);
        make_datatable(nullptr);
      }
    }


    void init_from_list_of_lists() {
      py::olist collist = src.to_pylist();
      check_names_count(collist.size());
      check_stypes_count(collist.size());
      for (size_t i = 0; i < collist.size(); ++i) {
        py::robj item = collist[i];
        SType s = get_stype_for_column(i);
        make_column(item, s);
      }
      make_datatable(names_arg);
    }


    void init_from_list_of_dicts_fixed_keys() {
      xassert(names_arg);
      py::olist srclist = src.to_pylist();
      py::olist nameslist = names_arg.to_pylist();
      size_t nrows = srclist.size();
      size_t ncols = nameslist.size();
      check_stypes_count(ncols);
      for (size_t i = 0; i < nrows; ++i) {
        py::robj item = srclist[i];
        if (!item.is_dict()) {
          throw TypeError() << "The source is not a list of dicts: element "
              << i << " is a " << item.typeobj();
        }
      }
      init_from_list_of_dicts_with_keys(nameslist);
    }


    void init_from_list_of_dicts_auto_keys() {
      xassert(!names_arg);
      if (stypes_arg && !stypes_arg.is_dict()) {
        throw TypeError() << "If the Frame() source is a list of dicts, then "
            "either the `names` list has to be provided explicitly, or "
            "`stypes` parameter has to be a dictionary (or missing)";
      }
      py::olist srclist = src.to_pylist();
      py::olist nameslist(0);
      py::oset  namesset;
      size_t nrows = srclist.size();
      for (size_t i = 0; i < nrows; ++i) {
        py::robj item = srclist[i];
        if (!item.is_dict()) {
          throw TypeError() << "The source is not a list of dicts: element "
              << i << " is a " << item.typeobj();
        }
        py::rdict row = item.to_rdict();
        for (auto kv : row) {
          py::robj& name = kv.first;
          if (!namesset.has(name)) {
            if (!name.is_string()) {
              throw TypeError() << "Invalid data in Frame() constructor: row "
                  << i << " dictionary contains a key of type "
                  << name.typeobj() << ", only string keys are allowed";
            }
            nameslist.append(name);
            namesset.add(name);
          }
        }
      }
      init_from_list_of_dicts_with_keys(nameslist);
    }


    void init_from_list_of_dicts_with_keys(const py::olist& nameslist) {
      py::olist srclist = src.to_pylist();
      size_t ncols = nameslist.size();
      for (size_t j = 0; j < ncols; ++j) {
        py::robj name = nameslist[j];
        SType s = get_stype_for_column(j, &name);
        Column* col = Column::from_pylist_of_dicts(srclist, name, int(s));
        cols.push_back(col);
      }
      make_datatable(nameslist);
    }


    void init_from_list_of_tuples() {
      py::olist srclist = src.to_pylist();
      py::rtuple item0 = srclist[0].to_rtuple_lax();
      size_t nrows = srclist.size();
      size_t ncols = item0.size();
      check_names_count(ncols);
      check_stypes_count(ncols);
      // Check that all entries are proper tuples
      for (size_t i = 0; i < nrows; ++i) {
        py::rtuple item = srclist[i].to_rtuple_lax();
        if (!item) {
          throw TypeError() << "The source is not a list of tuples: element "
              << i << " is a " << srclist[i].typeobj();
        }
        size_t this_ncols = item.size();
        if (this_ncols != ncols) {
          throw ValueError() << "Misshaped rows in Frame() constructor: "
              "row " << i << " contains " << this_ncols << " element"
              << (this_ncols == 1? "" : "s") << ", while "
              << (i == 1? "the previous row" : "previous rows")
              << " had " << ncols << " element" << (ncols == 1? "" : "s");
        }
      }
      // Create the columns
      for (size_t j = 0; j < ncols; ++j) {
        SType s = get_stype_for_column(j);
        cols.push_back(Column::from_pylist_of_tuples(srclist, j, int(s)));
      }
      if (names_arg || !item0.has_attr("_fields")) {
        make_datatable(names_arg);
      } else {
        make_datatable(item0.get_attr("_fields").to_pylist());
      }
    }


    void init_from_list_of_primitives() {
      check_names_count(1);
      check_stypes_count(1);
      SType s = get_stype_for_column(0);
      make_column(src.to_pyobj(), s);
      make_datatable(names_arg);
    }


    void init_from_dict() {
      if (defined_names) {
        throw TypeError() << "Parameter `names` cannot be used when "
            "constructing a Frame from a dictionary";
      }
      py::odict coldict = src.to_pydict();
      size_t ncols = coldict.size();
      check_stypes_count(ncols);
      strvec newnames;
      newnames.reserve(ncols);
      for (auto kv : coldict) {
        size_t i = newnames.size();
        py::robj name = kv.first;
        SType stype = get_stype_for_column(i, &name);
        newnames.push_back(name.to_string());
        make_column(kv.second, stype);
      }
      make_datatable(newnames);
    }


    void init_from_varkwds() {
      if (defined_names) {
        throw TypeError() << "Parameter `names` cannot be used when "
            "constructing a Frame from varkwd arguments";
      }
      size_t ncols = all_args.num_varkwd_args();
      check_stypes_count(ncols);
      strvec newnames;
      newnames.reserve(ncols);
      for (auto kv: all_args.varkwds()) {
        size_t i = newnames.size();
        const py::ostring oname(kv.first);
        SType stype = get_stype_for_column(i, &oname);
        newnames.push_back(std::move(kv.first));
        make_column(kv.second, stype);
      }
      make_datatable(newnames);
    }


    void init_mystery_frame() {
      cols.push_back(Column::from_range(42, 43, 1, SType::VOID));
      make_datatable(strvec { "?" });
    }


    void init_from_frame() {
      DataTable* srcdt = src.to_frame();
      size_t ncols = srcdt->ncols;
      check_names_count(ncols);
      if (stypes_arg || stype_arg) {
        // TODO: allow this use case
        throw TypeError() << "Parameter `stypes` is not allowed when making "
            "a copy of a Frame";
      }
      for (size_t i = 0; i < ncols; ++i) {
        cols.push_back(srcdt->columns[i]->shallowcopy());
      }
      if (names_arg) {
        make_datatable(names_arg.to_pylist());
      } else {
        make_datatable(srcdt);
      }
      if (srcdt->get_nkeys()) {
        frame->dt->set_nkeys_unsafe(srcdt->get_nkeys());
      }
    }


    void init_from_string() {
      py::otuple call_args(1);
      call_args.set(0, src.to_pyobj());

      py::oobj res = py::robj(py::fread_fn).call(call_args);
      if (res.is_frame()) {
        Frame* resframe = static_cast<Frame*>(res.to_borrowed_ref());
        std::swap(frame->dt,      resframe->dt);
        std::swap(frame->stypes,  resframe->stypes);
        std::swap(frame->ltypes,  resframe->ltypes);
        std::swap(frame->core_dt, resframe->core_dt);
        frame->core_dt->_frame = frame;
      } else {
        xassert(res.is_dict());
        auto err = ValueError();
        err << "Frame cannot be initialized from multiple source files: ";
        size_t i = 0;
        for (auto kv : res.to_pydict()) {
          if (i == 1) err << ", ";
          if (i == 2) { err << ", ..."; break; }
          err << '\'' << kv.first << '\'';
          ++i;
        }
        throw err;
      }
    }


    void init_from_pandas() {
      if (stypes_arg || stype_arg) {
        throw TypeError() << "Argument `stypes` is not supported in Frame() "
            "constructor when creating a Frame from pandas DataFrame";
      }
      py::robj pdsrc = src.to_pyobj();
      py::olist colnames(0);
      if (src.is_pandas_frame()) {
        py::oiter pdcols = pdsrc.get_attr("columns").to_pyiter();
        size_t ncols = pdcols.size();
        if (ncols != size_t(-1)) {
          check_names_count(ncols);
        }
        for (auto col : pdcols) {
          if (!names_arg) colnames.append(col.to_pystring_force());
          py::oobj colsrc = pdsrc.get_item(col).get_attr("values");
          make_column(colsrc, SType::VOID);
        }
      } else {
        xassert(src.is_pandas_series());
        check_names_count(1);
        py::oobj colsrc = pdsrc.get_attr("values");
        make_column(colsrc, SType::VOID);
      }
      if (colnames.size() > 0) {
        make_datatable(colnames);
      } else {
        make_datatable(names_arg);
      }
    }


    void init_from_numpy() {
      if (stypes_arg || stype_arg) {
        throw TypeError() << "Argument `stypes` is not supported in Frame() "
            "constructor when creating a Frame from a numpy array";
      }
      py::oobj npsrc = src.to_pyobj();
      size_t ndims = npsrc.get_attr("shape").to_pylist().size();
      if (ndims > 2) {
        throw ValueError() << "Cannot create Frame from a " << ndims << "-D "
            "numpy array " << npsrc;
      }
      if (ndims <= 1) {
        // This is equivalent to python `npsrc = npsrc.reshape(-1, 1)`
        // It changes the shape of the array, without altering the data
        npsrc = npsrc.invoke("reshape", "(ii)", -1, 1);
      }
      // Equivalent of python `npsrc.shape[1]`
      size_t ncols = npsrc.get_attr("shape").to_pylist()[1].to_size_t();
      check_names_count(ncols);

      py::otuple col_key(2);
      col_key.set(0, py::Ellipsis());
      if (npsrc.is_numpy_marray()) {
        for (size_t i = 0; i < ncols; ++i) {
          col_key.replace(1, py::oint(i));
          auto colsrc  = npsrc.get_attr("data").get_item(col_key);
          auto masksrc = npsrc.get_attr("mask").get_item(col_key);
          make_column(colsrc, SType::VOID);
          Column* maskcol = Column::from_buffer(masksrc);
          xassert(maskcol->stype() == SType::BOOL);
          cols.back()->apply_na_mask(static_cast<BoolColumn*>(maskcol));
          delete maskcol;
        }
      } else {
        for (size_t i = 0; i < ncols; ++i) {
          col_key.replace(1, py::oint(i));
          auto colsrc = npsrc.get_item(col_key);  // npsrc[:, i]
          make_column(colsrc, SType::VOID);
        }
      }
      make_datatable(names_arg);
    }



  //----------------------------------------------------------------------------
  // Helpers
  //----------------------------------------------------------------------------
  private:
    /**
     * Check that the number of names in `names_arg` corresponds to the number
     * of columns being created (`ncols`).
     */
    void check_names_count(size_t ncols) {
      if (!defined_names) return;
      size_t nnames = 0;
      if (names_arg.is_list_or_tuple()) {
        nnames = names_arg.to_pylist().size();
      }
      else {
        throw TypeError() << names_arg.name() << " should be a list of "
            "strings, instead received " << names_arg.typeobj();
      }
      if (nnames != ncols) {
        throw ValueError()
            << "The `names` argument contains " << nnames
            << " element" << (nnames==1? "" : "s") << ", which is "
            << (nnames < ncols? "less" : "more") << " than the number of "
               "columns being created (" << ncols << ")";
      }
    }


    void check_stypes_count(size_t ncols) {
      if (!defined_stypes) return;
      size_t nstypes = 0;
      if (stypes_arg.is_list_or_tuple()) {
        nstypes = stypes_arg.to_pylist().size();
      }
      else if (stypes_arg.is_dict()) {
        return;
      }
      else {
        throw TypeError() << stypes_arg.name() << " should be a list of "
            "stypes, instead received " << stypes_arg.typeobj();
      }
      if (nstypes != ncols) {
        throw ValueError()
            << "The `stypes` argument contains " << nstypes
            << " element" << (nstypes==1? "" : "s") << ", which is "
            << (nstypes < ncols? "less" : "more") << " than the number of "
               "columns being created (" << ncols << ")";
      }
    }


    /**
     * Retrieve the requested SType for column `i`. If the column's name is
     * known to the caller, it should be passed as the second parameter,
     * otherwise it will be retrieved from `names_arg` if necessary.
     *
     * If no SType is specified for the given column, this method returns
     * `SType::VOID`.
     *
     */
    SType get_stype_for_column(size_t i, const py::_obj* name = nullptr) {
      if (defined_stype) {
        return stype0;
      }
      if (defined_stypes) {
        if (stypes_arg.is_list_or_tuple()) {
          py::olist stypes = stypes_arg.to_pylist();
          return stypes[i].to_stype();
        }
        else {
          py::robj oname(nullptr);
          if (name == nullptr) {
            if (!defined_names) {
              throw TypeError() << "When parameter `stypes` is a dictionary, "
                  "column `names` must be explicitly specified";
            }
            py::olist names = names_arg.to_pylist();
            oname = names[i];
          } else {
            oname = *name;
          }
          py::odict stypes = stypes_arg.to_pydict();
          py::robj res = stypes.get(oname);
          if (res) {
            return res.to_stype();
          } else {
            return SType::VOID;
          }
        }
      }
      return SType::VOID;
    }


    Error _error_unknown_kwargs() {
      size_t n = all_args.num_varkwd_args();
      auto err = TypeError() << "Frame() constructor got ";
      if (n == 1) {
        err << "an unexpected keyword argument ";
        for (auto kv: all_args.varkwds()) {
          err << '\'' << kv.first << '\'';
        }
      } else {
        err << n << " unexpected keyword arguments: ";
        size_t i = 0;
        for (auto kv: all_args.varkwds()) {
          ++i;
          if (i <= 2 || i == n) {
            err << '\'' << kv.first << '\'';
            err << (i == n ? "" :
                    i == n - 1 ? " and " :
                    i == 1 ? ", " : ", ..., ");
          }
        }
      }
      return err;
    }


    void make_column(py::robj colsrc, SType s) {
      Column* col = nullptr;
      if (colsrc.is_buffer()) {
        col = Column::from_buffer(colsrc);
      }
      else if (colsrc.is_list_or_tuple()) {
        col = Column::from_pylist(colsrc.to_pylist(), int(s));
      }
      else if (colsrc.is_range()) {
        auto r = colsrc.to_pyrange();
        col = Column::from_range(r.start(), r.stop(), r.step(), s);
      }
      else {
        throw TypeError() << "Cannot create a column from " << colsrc.typeobj();
      }
      cols.push_back(col);
      if (cols.size() > 1) {
        size_t nrows0 = cols.front()->nrows;
        size_t nrows1 = cols.back()->nrows;
        if (nrows0 != nrows1) {
          throw ValueError()
            << "Column " << cols.size() - 1 << " has different number of "
            << "rows (" << nrows1 << ") than the preceding columns ("
            << nrows0 << ")";
        }
      }
    }


    void make_datatable(nullptr_t) {
      frame->dt = new DataTable(std::move(cols));
    }

    void make_datatable(const Arg& names) {
      if (names) {
        frame->dt = new DataTable(std::move(cols), names.to_pylist());
      } else {
        frame->dt = new DataTable(std::move(cols));
      }
    }

    void make_datatable(const py::olist& names) {
      frame->dt = new DataTable(std::move(cols), names);
    }

    void make_datatable(const std::vector<std::string>& names) {
      frame->dt = new DataTable(std::move(cols), names);
    }

    void make_datatable(const DataTable* names_src) {
      frame->dt = new DataTable(std::move(cols), names_src);
    }


    #ifdef DTTEST
      friend void dttest::cover_init_FrameInitializationManager_em();
    #endif
};



//------------------------------------------------------------------------------
// Custom error messages
//------------------------------------------------------------------------------

Error FrameInitializationManager::em::error_not_stype(PyObject*) const {
  return TypeError() << "Invalid value for `stype` parameter in Frame() "
                        "constructor";
} // LCOV_EXCL_LINE



//------------------------------------------------------------------------------
// Main Frame constructor
//------------------------------------------------------------------------------

void Frame::m__init__(PKArgs& args) {
  if (dt) m__dealloc__();
  dt = nullptr;
  core_dt = nullptr;
  stypes = nullptr;
  ltypes = nullptr;
  if (Frame::internal_construction) return;

  FrameInitializationManager fim(args, this);
  fim.run();

  core_dt = static_cast<pydatatable::obj*>(pydatatable::wrap(dt));
  core_dt->_frame = this;
}



//------------------------------------------------------------------------------
// pickling / unpickling
//------------------------------------------------------------------------------

static NoArgs fn___getstate__("__getstate__", nullptr);
static PKArgs fn___setstate__(1, 0, 0, false, false, {"state"},
                              "__setstate__", nullptr);


// TODO: add py::obytes object
oobj Frame::m__getstate__(const NoArgs&) {
  MemoryRange mr = dt->save_jay();
  auto data = static_cast<const char*>(mr.xptr());
  auto size = static_cast<Py_ssize_t>(mr.size());
  return oobj::from_new_reference(PyBytes_FromStringAndSize(data, size));
}

void Frame::m__setstate__(const PKArgs& args) {
  PyObject* _state = args[0].to_borrowed_ref();
  if (!PyBytes_Check(_state)) {
    throw TypeError() << "`__setstate__()` expects a bytes object";
  }
  // Clean up any previous state of the Frame (since pickle first creates an
  // empty Frame object, and then calls __setstate__ on it).
  m__dealloc__();
  core_dt = nullptr;
  stypes = nullptr;
  ltypes = nullptr;

  const char* data = PyBytes_AS_STRING(_state);
  size_t length = static_cast<size_t>(PyBytes_GET_SIZE(_state));
  dt = open_jay_from_bytes(data, length);
  PyObject* _dt = pydatatable::wrap(dt);
  if (!_dt) throw PyError();
  core_dt = reinterpret_cast<pydatatable::obj*>(_dt);
  core_dt->_frame = this;
}


void Frame::Type::_init_init(Methods& mm, GetSetters&) {
  mm.add<&Frame::m__getstate__, fn___getstate__>();
  mm.add<&Frame::m__setstate__, fn___setstate__>();
}


}  // namespace py



//------------------------------------------------------------------------------
// Testing
//------------------------------------------------------------------------------

// This test ensures coverage for `_ZN2py26FrameInitializationManager2emD0Ev`
// symbol. See https://stackoverflow.com/questions/46447674 for details.
#ifdef DTTEST
namespace dttest {

  void cover_init_FrameInitializationManager_em() {
    auto t = new py::FrameInitializationManager::em;
    delete t;
  }

}
#endif

// Two lines in the file are marked as LCOV_EXCL_LINE: these lines are related
// to auto-generated exception-handling code, and they are not covered because
// those exceptions are almost impossible to trigger.
// See https://stackoverflow.com/questions/46367192
