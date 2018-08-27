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
#include "python/string.h"
#include "utils/alloc.h"

namespace py {


//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

// TODO: this functionality should really be rolled into the DataTable class.
class DTMaker {
  private:
    std::vector<Column*> cols;

  public:
    DTMaker() = default;
    ~DTMaker() {
      for (auto col : cols) dt::free(col);
    }

    void push_back(Column* col) {
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

    DataTable* to_datatable() {
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
};


/*
static std::vector<std::string> _get_names(const Arg& arg) {
  if (arg.is_undefined() || arg.is_none()) {
    return std::vector<std::string>();
  }
  // if (arg.is_list() || arg.is_tuple()) {
  //   return arg.to_list_of_strs();
  // }
  // TODO: also allow a single-column Frame of string type
  throw TypeError() << arg.name() << " must be a list/tuple of column names";
}
*/


static Column* _make_column_from_listlike(py::obj src) {
  if (src.is_buffer()) {
    return Column::from_buffer(src.to_borrowed_ref());
  }
  else if (src.is_list()) {
    return Column::from_pylist(src.to_pylist(), 0);
  }
  else if (src.is_range()) {
    // return Column::from_range(src.to_range());
  }
  return nullptr;
}


static DataTable* _make_frame_from_list_of_listlike(py::olist list) {
  DTMaker dtm;
  size_t ncols = list.size();
  for (size_t i = 0; i < ncols; ++i) {
    Column* col = _make_column_from_listlike(list[i]);
    dtm.push_back(col);
  }
  return dtm.to_datatable();
}


static DataTable* _make_frame_from_list(py::olist list) {
  if (list.size() == 0) {
    return DTMaker().to_datatable();
  }
  py::obj item0 = list[0];
  if (item0.is_list() || item0.is_range()) {
    return _make_frame_from_list_of_listlike(list);
  }
  // else if (item0.is_dict()) {
  //   return _make_frame_from_list_of_dicts(list);
  // }
  // else if (item0.is_tuple()) {
  //   return _make_frame_from_list_of_tuples(list);
  // }
  else {
    DTMaker dtm;
    dtm.push_back(_make_column_from_listlike(list));
    return dtm.to_datatable();
  }
}



//------------------------------------------------------------------------------
// Main constructor
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

  // bool names_defined = !names_arg.is_undefined();
  // auto names = _get_names(names_arg);

  // if (src.is_list_or_tuple()) {
  //   dt = _make_frame_from_list(src.to_pylist());
  // }
  // else
  if ((src.is_undefined() || src.is_none()) && args.num_varkwd_args() == 0) {
    dt = DTMaker().to_datatable();
  }

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
