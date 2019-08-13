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
#include "expr/collist.h"
#include "expr/expr.h"
#include "expr/expr_column.h"
#include "expr/expr_columnset.h"
#include "expr/workframe.h"
#include "utils/exceptions.h"
#include "datatable.h"
namespace dt {


using stypevec = std::vector<SType>;

static const strvec typenames =
  {"?", "boolean", "integer", "string", "expr", "type"};

static stypevec stBOOL = {SType::BOOL};
static stypevec stINT = {SType::INT8, SType::INT16, SType::INT32, SType::INT64};
static stypevec stFLOAT = {SType::FLOAT32, SType::FLOAT64};
static stypevec stSTR = {SType::STR32, SType::STR64};
static stypevec stOBJ = {SType::OBJ};


template <typename T>
static void concat_vectors(std::vector<T>& a, std::vector<T>&& b) {
  a.reserve(a.size() + b.size());
  for (size_t i = 0; i < b.size(); ++i) {
    a.push_back(std::move(b[i]));
  }
}



//------------------------------------------------------------------------------
// collist_maker
//------------------------------------------------------------------------------

class collist_maker
{
  public:
    enum class list_type : size_t {
      UNKNOWN, BOOL, INT, STR, EXPR, TYPE
    };

    workframe& wf;
    const DataTable* dt0;
    const char* srcname;
    list_type type;
    intvec  indices;
    exprvec exprs;
    strvec  names;
    size_t  k;  // The index of the current element
    bool is_update;
    bool is_delete;
    bool has_new_columns;
    size_t : 40;

  public:
    collist_maker(workframe& wf_, EvalMode mode, const char* srcname_,
                  size_t dt_index)
      : wf(wf_)
    {
      is_update = (mode == EvalMode::UPDATE);
      is_delete = (mode == EvalMode::DELETE);
      has_new_columns = false;
      dt0 = wf.get_datatable(dt_index);
      type = list_type::UNKNOWN;
      k = 0;
      srcname = srcname_;
    }


    void process(py::robj src) {
      if (src.is_dtexpr())      _process_element_expr(src);
      else if (src.is_int())    _process_element_int(src);
      else if (src.is_string()) _process_element_string(src);
      else if (src.is_slice())  _process_element_slice(src);
      else if (src.is_type())   _process_element_type(src);
      else if (src.is_ltype())  _process_element_ltype(src);
      else if (src.is_stype())  _process_element_stype(src);

      else if (src.is_list_or_tuple()) {
        py::olist srclist = src.to_pylist();
        size_t nelems = srclist.size();
        for (size_t i = 0; i < nelems; ++i) {
          _process_element(srclist[i]);
        }
      }
      else if (src.is_dict()) {
        if (is_delete) {
          throw TypeError() << "When del operator is applied, "
              << srcname << " cannot be a dictionary";
        }
        type = list_type::EXPR;
        for (auto kv : src.to_pydict()) {
          if (!kv.first.is_string()) {
            throw TypeError()
                << "Keys in " << srcname << " dictionary must be strings";
          }
          names.push_back(kv.first.to_string());
          _process_element(kv.second);
        }
      }
      else if (src.is_generator()) {
        for (auto elem : src.to_oiter()) {
          _process_element(elem);
        }
      }
      else if (!src.is_none()) {
        throw TypeError()
          << "Unsupported " << srcname << " of type " << src.typeobj();
      }

      // Finalize
      if (type == list_type::BOOL && k != dt0->ncols) {
        throw ValueError()
            << "The length of boolean list in " << srcname
            << " does not match the number of columns in the Frame: "
            << k << " vs " << dt0->ncols;
      }
    }


  private:
    void _set_type(list_type t) {
      if (type == list_type::UNKNOWN) type = t;
      if (type == t) return;
      if (k) {
        throw TypeError()
            << "Mixed selector types in " << srcname
            << " are not allowed. Element " << k << " is of type "
            << typenames[static_cast<size_t>(t)]
            << ", whereas the previous element(s) were of type "
            << typenames[static_cast<size_t>(type)];
      } else {
        throw TypeError()
            << "The values in " << srcname
            << " dictionary must be expressions, not "
            << typenames[static_cast<size_t>(t)] << 's';
      }
    }

    void _process_element(py::robj elem) {
      if (elem.is_dtexpr())      _process_element_expr(elem);
      else if (elem.is_int())    _process_element_int(elem);
      else if (elem.is_bool())   _process_element_bool(elem);
      else if (elem.is_string()) _process_element_string(elem);
      else if (elem.is_slice())  _process_element_slice(elem);
      else if (elem.is_type())   _process_element_type(elem);
      else if (elem.is_ltype())  _process_element_ltype(elem);
      else if (elem.is_stype())  _process_element_stype(elem);
      else if (elem.is_none()) return;
      else {
        throw TypeError()
            << "Element " << k << " in " << srcname << " list has type `"
            << elem.typeobj() << "`, which is not supported";
      }
      ++k;
    }

    void _process_element_int(py::robj elem) {
      _set_type(list_type::INT);
      int64_t i = elem.to_int64_strict();
      int64_t icols = static_cast<int64_t>(dt0->ncols);
      if (i < -icols || i >= icols) {
        throw ValueError()
            << "Column index `" << i << "` is invalid for a Frame with "
            << icols << " column" << (icols == 1? "" : "s");
      }
      if (i < 0) i += icols;
      indices.push_back(static_cast<size_t>(i));
    }

    void _process_element_bool(py::robj elem) {
      _set_type(list_type::BOOL);
      int8_t t = elem.to_bool_strict();
      if (t) {
        indices.push_back(k);
      }
    }

    void _process_element_string(py::robj elem) {
      _set_type(list_type::STR);
      if (is_update) {
        int64_t j = dt0->colindex(elem);
        if (j == -1) {
          has_new_columns = true;
          names.resize(indices.size());
          names.push_back(elem.to_string());
        }
        indices.push_back(static_cast<size_t>(j));
      } else {
        size_t j = dt0->xcolindex(elem);
        indices.push_back(j);
      }
    }

    void _process_element_expr(py::robj elem) {
      _set_type(list_type::EXPR);
      auto expr = elem.to_dtexpr();
      if (expr->is_columnset_expr()) {
        auto csexpr = dynamic_cast<expr::expr_columnset*>(expr.get());
        auto collist = csexpr->convert_to_collist(wf);
        concat_vectors(names,   collist->release_names());
        concat_vectors(indices, collist->release_indices());
        concat_vectors(exprs,   collist->release_exprs());
        return;
      }
      if (expr->is_column_expr()) {
        size_t frid = expr->get_col_frame(wf);
        if (frid == 0) {
          size_t i = expr->get_col_index(wf);
          indices.push_back(i);
        }
        else if (frid >= wf.nframes()) {
          throw ValueError() << "Item " << k << " of " << srcname << " list "
              "references a non-existing join frame";
        }
      }
      exprs.push_back(std::move(expr));
    }

    void _process_element_slice(py::robj elem) {
      py::oslice ssrc = elem.to_oslice();
      if (ssrc.is_numeric()) return _process_element_numslice(ssrc);
      if (ssrc.is_string())  return _process_element_strslice(ssrc);
      throw TypeError() << ssrc << " is neither integer- nor string-valued";
    }

    void _process_element_numslice(py::oslice ssrc) {
      _set_type(list_type::INT);
      size_t len = dt0->ncols;
      size_t start, count, step;
      ssrc.normalize(len, &start, &count, &step);
      indices.reserve(indices.size() + count);
      for (size_t i = 0; i < count; ++i) {
        indices.push_back(start + i * step);
      }
    }

    void _process_element_strslice(py::oslice ssrc) {
      _set_type(list_type::STR);
      py::oobj ostart = ssrc.start_obj();
      py::oobj ostop = ssrc.stop_obj();
      size_t start = ostart.is_none()? 0 : dt0->xcolindex(ostart);
      size_t end = ostop.is_none()? dt0->ncols - 1 : dt0->xcolindex(ostop);
      size_t len = start <= end? end - start + 1 : start - end + 1;
      indices.reserve(len);
      if (start <= end) {
        for (size_t i = start; i <= end; ++i) {
          indices.push_back(i);
        }
      } else {
        // Careful with the case when `end = 0`. In this case a regular for-loop
        // `(i = start; i >= end; --i)` becomes infinite.
        size_t i = start;
        do {
          indices.push_back(i);
        } while (i-- != end);
      }
    }

    void _process_element_type(py::robj elem) {
      _set_type(list_type::TYPE);
      PyTypeObject* et = reinterpret_cast<PyTypeObject*>(elem.to_borrowed_ref());
      if (et == &PyLong_Type)            _select_types(stINT);
      else if (et == &PyFloat_Type)      _select_types(stFLOAT);
      else if (et == &PyUnicode_Type)    _select_types(stSTR);
      else if (et == &PyBool_Type)       _select_types(stBOOL);
      else if (et == &PyBaseObject_Type) _select_types(stOBJ);
      else {
        throw ValueError()
            << "Unknown type " << elem << " used as " << srcname;
      }
    }

    void _process_element_ltype(py::robj elem) {
      _set_type(list_type::TYPE);
      size_t lt = elem.get_attr("value").to_size_t();
      switch (static_cast<LType>(lt)) {
        case LType::BOOL:   _select_types(stBOOL); break;
        case LType::INT:    _select_types(stINT); break;
        case LType::REAL:   _select_types(stFLOAT); break;
        case LType::STRING: _select_types(stSTR); break;
        case LType::OBJECT: _select_types(stOBJ); break;
        default:
          throw TypeError() << "Unknown ltype value " << lt;
      }
    }

    void _process_element_stype(py::robj elem) {
      _set_type(list_type::TYPE);
      size_t st = elem.get_attr("value").to_size_t();
      _select_types({static_cast<SType>(st)});
    }

    void _select_types(const std::vector<SType>& stypes) {
      size_t ncols = dt0->ncols;
      for (size_t i = 0; i < ncols; ++i) {
        SType st = dt0->get_ocolumn(i).stype();
        for (SType s : stypes) {
          if (s == st) {
            indices.push_back(i);
            break;
          }
        }
      }
    }
};



//------------------------------------------------------------------------------
// collist
//------------------------------------------------------------------------------

collist::collist(workframe& wf, py::robj src, const char* srcname,
                 size_t dt_index)
{
  collist_maker maker(wf, wf.get_mode(), srcname, dt_index);
  maker.process(src);
  exprs = std::move(maker.exprs);
  names = std::move(maker.names);
  indices = std::move(maker.indices);
  // A list of "EXPR" type may be either a list of plain column selectors
  // (such as `f.A`), or a list of more complicated expressions. In the
  // former case the vector of `indices` will be the same size as `exprs`,
  // and we return a `collist_jn` node. In the latter case, the
  // `exprlist_jn` node is created.
  if (exprs.size() > indices.size()) {
    xassert(maker.type == collist_maker::list_type::EXPR);
    indices.clear();
  }
}


bool collist::is_simple_list() const {
  return (indices.size() > 0) || (exprs.size() == 0);
}


strvec collist::release_names()   { return std::move(names); }
intvec collist::release_indices() { return std::move(indices); }
exprvec collist::release_exprs()  { return std::move(exprs); }






} // namespace dt
