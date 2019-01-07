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
#include "expr/base_expr.h"
#include "expr/j_node.h"
#include "expr/workframe.h"   // dt::workframe
namespace dt {


class col_set {
  private:
    using ripair = std::pair<RowIndex, RowIndex>;
    colvec columns;
    std::vector<ripair> all_ri;

  public:
    void reserve_extra(size_t n) {
      columns.reserve(columns.size() + n);
    }
    void add_column(const Column* col, const RowIndex& ri) {
      const RowIndex& ricol = col->rowindex();
      Column* newcol = col->shallowcopy(rowindex_product(ri, ricol));
      columns.push_back(newcol);
    }
    colvec&& release() {
      return std::move(columns);
    }

  private:
    RowIndex& rowindex_product(const RowIndex& ra, const RowIndex& rb) {
      for (auto it = all_ri.rbegin(); it != all_ri.rend(); ++it) {
        if (it->first == ra) {
          return it->second;
        }
      }
      all_ri.push_back(std::make_pair(ra, ra * rb));
      return all_ri.back().second;
    }
};




//------------------------------------------------------------------------------
// allcols_jn
//------------------------------------------------------------------------------

/**
 * j_node representing selection of all columns (i.e. `:`). This is roughly
 * the equivalent of SQL's "*".
 *
 * select()
 *   In the simplest case, this node selects all columns from the source Frame.
 *   The groupby field, if present, is ignored and the columns are selected
 *   as-is, applying the RowIndex that was already computed. The names of the
 *   selected columns will be exactly the same as in the source Frame.
 *
 *   However, when 2 or more Frames are joined, this selector will select all
 *   columns from all joined Frames. The exception to this are natural joins,
 *   where the key columns of joined Frames will be excluded from the result.
 *
 * delete()
 *   Even if several frames are joined, the delete() operator applies only to
 *   the "main" subframe.
 *   When `j` expression selects all columns, the delete() operator removes
 *   the rows from a Frame. This is achieved by computing the Rowindex implied
 *   by the `i` expression, then negating that Rowindex and applying it to the
 *   source frame.
 *   However, when `i` is "all rows", then deleting all rows + all columns
 *   completely empties the Frame: its shape becomes [0 x 0].
 */
class allcols_jn : public j_node {
  public:
    allcols_jn() = default;
    DataTable* select(workframe&) override;
    void delete_(workframe&) override;
};


DataTable* allcols_jn::select(workframe& wf) {
  col_set cols;
  strvec names;
  for (size_t i = 0; i < wf.nframes(); ++i) {
    const DataTable* dti = wf.get_datatable(i);
    const RowIndex& rii = wf.get_rowindex(i);

    size_t j0 = wf.is_naturally_joined(i)? dti->get_nkeys() : 0;
    cols.reserve_extra(dti->ncols - j0);
    if (wf.nframes() > 1) {
      const strvec& dti_names = dti->get_names();
      names.insert(names.end(), dti_names.begin() + j0, dti_names.end());
    }

    for (size_t j = j0; j < dti->ncols; ++j) {
      cols.add_column(dti->columns[j], rii);
    }
  }
  if (wf.nframes() == 1) {
    // Copy names from the source DataTable
    return new DataTable(cols.release(), wf.get_datatable(0));
  }
  else {
    // Otherwise the names must be potentially de-duplicated
    return new DataTable(cols.release(), std::move(names));
  }
}


void allcols_jn::delete_(workframe& wf) {
  DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  if (ri0) {
    RowIndex ri_neg = ri0.negate(dt0->nrows);
    dt0->apply_rowindex(ri_neg);
  } else {
    dt0->delete_all();
  }
}




//------------------------------------------------------------------------------
// collist_jn
//------------------------------------------------------------------------------

/**
 * This is a j node representing a plain selection of columns from the source
 * Frame. This node cannot be used to select columns from any joined frames
 * (although those are still allowed in the evaluation graph).
 *
 * select()
 *   The columns at stored indices are selected into a new DataTable. The
 *   RowIndex, if any, is applied to all these columns. The joined frames are
 *   ignored, as well as any groupby information.
 *
 * delete()
 *   When `i` node is `allrows_in`, then the columns at given indices are
 *   deleted (the indices should also be deduplicated). Otherwise, the
 *   deletion region is a subset of rows/columns, and we just set the values
 *   at those places to NA.
 *
 */
class collist_jn : public j_node {
  private:
    std::vector<size_t> indices;
    strvec names;

  public:
    collist_jn(std::vector<size_t>&& cols, strvec&& names_);
    DataTable* select(workframe& wf) override;
    void delete_(workframe&) override;
};


collist_jn::collist_jn(std::vector<size_t>&& cols, strvec&& names_)
  : indices(std::move(cols)), names(std::move(names_))
{
  xassert(names.empty() || names.size() == indices.size());
}


DataTable* collist_jn::select(workframe& wf) {
  const DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  if (names.empty()) {
    const strvec& dt0_names = dt0->get_names();
    names.reserve(indices.size());
    for (size_t i : indices) {
      names.push_back(dt0_names[i]);
    }
  }
  col_set cols;
  cols.reserve_extra(indices.size());
  for (size_t i : indices) {
    cols.add_column(dt0->columns[i], ri0);
  }
  return new DataTable(cols.release(), std::move(names));
}


void collist_jn::delete_(workframe& wf) {
  DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  if (ri0) {
    for (size_t i : indices) {
      dt0->columns[i]->replace_values(ri0, nullptr);
    }
  } else {
    dt0->delete_columns(indices);
  }
}



//------------------------------------------------------------------------------
// exprlist_jn
//------------------------------------------------------------------------------
using exprvec = std::vector<std::unique_ptr<dt::base_expr>>;

class exprlist_jn : public j_node {
  private:
    exprvec exprs;
    strvec names;

  public:
    exprlist_jn(exprvec&& cols, strvec&& names_);
    DataTable* select(workframe& wf) override;
    void delete_(workframe&) override;
};


exprlist_jn::exprlist_jn(exprvec&& cols, strvec&& names_)
  : exprs(std::move(cols)), names(std::move(names_))
{
  xassert(names.empty() || names.size() == exprs.size());
}


DataTable* exprlist_jn::select(workframe& wf) {
  if (names.empty()) {
    names.resize(exprs.size());
  }
  col_set cols;
  cols.reserve_extra(exprs.size());
  for (auto& expr : exprs) {
    expr->resolve(wf);
  }
  RowIndex ri0;  // empty rowindex
  for (auto& expr : exprs) {
    cols.add_column(expr->evaluate_eager(wf), ri0);
  }
  return new DataTable(cols.release(), std::move(names));
}


void exprlist_jn::delete_(workframe&) {
  for (size_t i = 0; i < exprs.size(); ++i) {
    auto colexpr = dynamic_cast<dt::expr_column*>(exprs[i].get());
    if (!colexpr) {
      throw TypeError() << "Item " << i << " in the `j` selector list is a "
        "computed expression and cannot be deleted";
    }
    if (colexpr->get_frame_id() > 0) {
      throw TypeError() << "Item " << i << " in the `j` selector list is a "
        "column from a joined frame and cannot be deleted";
    }
  }
  // An `exprlist_jn` cannot contain all exprs that are `expr_column`s and their
  // frame_id is 0. Such node should have been created as `collist_jn` instead.
  xassert(false);  // LCOV_EXCL_LINE
}




//------------------------------------------------------------------------------
// j_node factory functions
//------------------------------------------------------------------------------
using intvec = std::vector<size_t>;
using stypevec = std::vector<SType>;

static const strvec typenames =
  {"?", "boolean", "integer", "string", "expr", "type"};

static stypevec stBOOL = {SType::BOOL};
static stypevec stINT = {SType::INT8, SType::INT16, SType::INT32, SType::INT64};
static stypevec stFLOAT = {SType::FLOAT32, SType::FLOAT64};
static stypevec stSTR = {SType::STR32, SType::STR64};
static stypevec stOBJ = {SType::OBJ};


class jnode_maker
{
  private:
    enum class list_type : size_t {
      UNKNOWN, BOOL, INT, STR, EXPR, TYPE
    };

    workframe& wf;
    const DataTable* dt0;
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
    jnode_maker(py::robj src, workframe& wf_) : wf(wf_)
    {
      is_update = (wf.get_mode() == EvalMode::UPDATE);
      is_delete = (wf.get_mode() == EvalMode::DELETE);
      dt0 = wf.get_datatable(0);
      type = list_type::UNKNOWN;
      k = 0;

      if (is_PyBaseExpr(src))   _process_element_expr(src);
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
          throw TypeError() << "When del operator is applied, the `j` selector "
              "cannot be a dictionary";
        }
        type = list_type::EXPR;
        for (auto kv : src.to_pydict()) {
          if (!kv.first.is_string()) {
            throw TypeError()
                << "Keys in the `j` selector dictionary must be strings";
          }
          names.push_back(kv.first.to_string());
          _process_element(kv.second);
        }
      }
      else if (src.is_iterable() && !src.is_bytes()) {
        for (auto elem : src.to_oiter()) {
          _process_element(elem);
        }
      }
      else {
        throw TypeError()
          << "Unsupported `j` selector of type " << src.typeobj();
      }
    }

    j_node* get_node() {
      if (type == list_type::BOOL && k != dt0->ncols) {
        throw ValueError()
            << "The length of boolean list in `j` does not match the "
               "number of columns in the Frame: "
            << k << " vs " << dt0->ncols;
      }
      // A list of "EXPR" type may be either a list of plain column selectors
      // (such as `f.A`), or a list of more complicated expressions. In the
      // former case the vector of `indices` will be the same size as `exprs`,
      // and we return a `collist_jn` node. In the latter case, the
      // `exprlist_jn` node is created.
      if (exprs.size() > indices.size()) {
        xassert(type == list_type::EXPR);
        return new exprlist_jn(std::move(exprs), std::move(names));
      }
      return new collist_jn(std::move(indices), std::move(names));
    }

  private:
    void _set_type(list_type t) {
      if (type == list_type::UNKNOWN) type = t;
      if (type == t) return;
      if (k) {
        throw TypeError()
            << "Mixed selector types in `j` are not allowed. Element "
            << k << " is of type "
            << typenames[static_cast<size_t>(t)]
            << ", whereas the previous element(s) were of type "
            << typenames[static_cast<size_t>(type)];
      } else {
        throw TypeError()
            << "The values in the `j` selector dictionary must be "
               "expressions, not "
            << typenames[static_cast<size_t>(t)] << 's';
      }
    }

    void _process_element(py::robj elem) {
      if (is_PyBaseExpr(elem))   _process_element_expr(elem);
      else if (elem.is_int())    _process_element_int(elem);
      else if (elem.is_bool())   _process_element_bool(elem);
      else if (elem.is_string()) _process_element_string(elem);
      else if (elem.is_slice())  _process_element_slice(elem);
      else if (elem.is_type())   _process_element_type(elem);
      else if (elem.is_ltype())  _process_element_ltype(elem);
      else if (elem.is_stype())  _process_element_stype(elem);
      else {
        throw TypeError()
            << "Element " << k << " in the `j` selector list has type `"
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
      py::oobj res = elem.invoke("_core");
      xassert(res.typeobj() == &py::base_expr::Type::type);
      auto pybe = reinterpret_cast<py::base_expr*>(res.to_borrowed_ref());
      dt::base_expr* pexpr = pybe->release();
      auto expr = std::unique_ptr<dt::base_expr>(pexpr);

      dt::expr_column* colexpr = dynamic_cast<dt::expr_column*>(pexpr);
      if (colexpr) {
        size_t frid = colexpr->get_frame_id();
        if (frid == 0) {
          size_t i = colexpr->get_col_index(wf);
          indices.push_back(i);
        }
        else if (frid >= wf.nframes()) {
          throw ValueError() << "Item " << k << " of the `j` selector list "
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
            << "Unknown type " << elem << " used as a `j` selector";
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
        SType st = dt0->columns[i]->stype();
        for (SType s : stypes) {
          if (s == st) {
            indices.push_back(i);
            break;
          }
        }
      }
    }
};


static j_node* _make(py::robj src, workframe& wf) {
  // The most common case is ":", a trivial slice
  if ((src.is_slice() && src.to_oslice().is_trivial())
      || src.is_none() || src.is_ellipsis()) {
    return new allcols_jn();
  }
  return jnode_maker(src, wf).get_node();
}




//------------------------------------------------------------------------------
// j_node
//------------------------------------------------------------------------------

jptr j_node::make(py::robj src, workframe& wf) {
  return jptr(_make(src, wf));
}

j_node::~j_node() {}

void j_node::update(workframe&) {}



}  // namespace dt
