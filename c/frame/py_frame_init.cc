//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include <iostream>
#include <string>
#include <vector>
#include "python/dict.h"
#include "python/list.h"
#include "python/orange.h"
#include "python/string.h"
#include "utils/alloc.h"

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
    Frame* frame;
    std::vector<Column*> cols;


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
        frame(f) {}

    ~FrameInitializationManager() {
      for (auto col : cols) dt::free(col);
    }


    void run()
    {
      if (all_args.num_varkwd_args()) {
        if (src) {
          throw _error_unknown_kwargs();
        }
        _init_from_varkwd_dict();
      }
      else if (src.is_list_or_tuple()) {
        py::olist collist = src.to_pylist();
        if (collist.size() == 0) {
          _init_empty_frame();
        }
        else {
          py::obj item0 = collist[0];
          if (item0.is_list() || item0.is_range() || item0.is_buffer()) {
            _init_from_list_of_lists();
          }
          else if (item0.is_dict()) {
            _init_from_list_of_dicts();
          }
          else if (item0.is_tuple()) {
            _init_from_list_of_tuples();
          }
          else {
            _init_from_list_of_primitives();
          }
        }
      }
      else if (src.is_dict()) {
        _init_from_dict();
      }
      else if (src.is_undefined() || src.is_none()) {
        _init_empty_frame();
      }
    }


  //----------------------------------------------------------------------------
  // Helpers
  //----------------------------------------------------------------------------
  private:
    /**
     * Check that the number of names in `names_arg` corresponds to the number
     * of columns created (`ncols`).
     */
    void _check_names(size_t ncols) {
      if (names_arg.is_undefined() || names_arg.is_none()) {
        return;
      }
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
            << "`names` argument contains " << nnames
            << " element" << (nnames==1? "" : "s") << ", which is "
            << (nnames < ncols? "less" : "more") << " than the number of "
               "columns being created (" << ncols << ")";
      }
    }


    void _check_stypes(size_t ncols) {
      if (stypes_arg.is_undefined() || stypes_arg.is_none()) {
        return;
      }
      if (!(stype_arg.is_undefined() || stype_arg.is_none())) {
        throw TypeError() << "Parameters `stypes` and `stype` cannot be "
            "passed to Frame() simultaneously";
      }
      size_t nstypes = 0;
      if (stypes_arg.is_list_or_tuple()) {
        nstypes = stypes_arg.to_pylist().size();
      }
      else {
        throw TypeError() << stypes_arg.name() << " should be a list of "
            "stypes, instead received " << stypes_arg.typeobj();
      }
      if (nstypes != ncols) {
        throw ValueError()
            << "`stypes` argument contains " << nstypes
            << " element" << (nstypes==1? "" : "s") << ", which is "
            << (nstypes < ncols? "less" : "more") << " than the number of "
               "columns being created (" << ncols << ")";
      }
    }


    void _add_col(Column* col) {
      cols.push_back(col);
      if (cols.size() > 1) {
        int64_t nrows0 = cols.front()->nrows;
        int64_t nrows1 = cols.back()->nrows;
        if (nrows0 != nrows1) {
          throw ValueError()
            << "Column " << cols.size() - 1 << " has different number of "
            << "rows (" << nrows1 << ") than the preceding columns ("
            << nrows0 << ")";
        }
      }
    }

    DataTable* _make_datatable() {
      size_t ncols = cols.size();
      size_t allocsize = sizeof(Column*) * (ncols + 1);
      Column** newcols = dt::malloc<Column*>(allocsize);
      if (ncols) {
        std::memcpy(newcols, cols.data(), sizeof(Column*) * ncols);
      }
      newcols[ncols] = nullptr;
      try {
        DataTable* res = new DataTable(newcols);
        cols.clear();
        return res;
      } catch (const std::exception&) {
        dt::free(newcols);
        throw;
      }
    }


  //----------------------------------------------------------------------------
  // Frame creation methods
  //----------------------------------------------------------------------------
  private:
    void _init_empty_frame() {
      _check_names(0);
      _check_stypes(0);
      frame->dt = _make_datatable();
    }

    void _init_from_list_of_lists() {
      py::olist collist = src.to_pylist();
      // check for collist.size() names & stypes
      for (size_t i = 0; i < collist.size(); ++i) {
        py::obj item = collist[i];
        // get stype for index `i`
        // add column built from `item`
      }
      // DTMaker dtm;
      // size_t ncols = list.size();
      // for (size_t i = 0; i < ncols; ++i) {
      //   Column* col = _make_column_from_listlike(list[i]);
      //   dtm.push_back(col);
      // }
      // return dtm.to_datatable();
    }

    void _init_from_list_of_dicts() {
      // TODO
    }

    void _init_from_list_of_tuples() {
      // TODO
    }

    void _init_from_list_of_primitives() {
      py::olist colsrc = src.to_pylist();
      // check for 1 name
      // check for 1 stype
      // get stype (0)
      // add column made from `colsrc`
    }

    void _init_from_dict() {
      // check for 0 names
      py::odict coldict = src.to_pydict();
      // check for coldict.size() stypes
      for (auto kv : coldict) {
        // add name kv.first
        // get stype by name(kv.first)
        // add column from kv.second
      }
    }

    void _init_from_varkwd_dict() {
      // TODO
    }

    Error _error_unknown_kwargs() {
      return TypeError() << "Uknown keyword arguments";
    }

    /*
    Column* _make_column_from_listlike(py::obj src) {
      if (src.is_buffer()) {
        return Column::from_buffer(src.to_borrowed_ref());
      }
      else if (src.is_list()) {
        return Column::from_pylist(src.to_pylist(), 0);
      }
      else if (src.is_range()) {
        auto r = src.to_pyrange();
        return Column::from_range(r.start(), r.stop(), r.step(), SType::VOID);
      }
      return nullptr;
    }
    */
};



//------------------------------------------------------------------------------
// Main Frame constructor
//------------------------------------------------------------------------------

void Frame::m__init__(PKArgs& args) {
  const Arg& src        = args[0];
  const Arg& names_arg  = args[1];
  const Arg& stypes_arg = args[2];
  const Arg& stype_arg  = args[3];

  dt = nullptr;
  core_dt = nullptr;
  stypes = nullptr;
  ltypes = nullptr;
  names = nullptr;
  inames = nullptr;

  FrameInitializationManager fim(args, this);
  fim.run();

  if (dt) {
    core_dt = static_cast<pydatatable::obj*>(pydatatable::wrap(dt));
  } else {
    py::oobj osrc(src.to_borrowed_ref());
    py::oobj ostypes(stypes_arg.to_borrowed_ref());
    if (stype_arg) {
      py::olist stypes_list(1);
      stypes_list.set(0, stype_arg.to_borrowed_ref());
      ostypes = stypes_list;
    }
    if (args.num_varkwd_args()) {
      if (src) {
        throw TypeError() << "Unknown keyword arguments in Frame.__init__()";
      }
      py::odict kvdict;
      for (auto kv : args.varkwds()) {
        kvdict.set(ostring(kv.first), kv.second);
      }
      osrc = std::move(kvdict);
    }
    PyObject* arg1 = osrc.to_borrowed_ref();
    PyObject* arg2 = names_arg.to_borrowed_ref();
    PyObject* arg3 = ostypes.to_borrowed_ref();
    if (!arg1) arg1 = py::None().release();
    if (!arg2) arg2 = py::None().release();
    if (!arg3) arg3 = py::None().release();
    py::obj(this).invoke("_fill_from_source", "OOO", arg1, arg2, arg3);
  }
}


}  // namespace py
