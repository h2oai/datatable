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
        if (it->first == rb) {
          return it->second;
        }
      }
      all_ri.push_back(std::make_pair(rb, ra * rb));
      return all_ri.back().second;
    }
};


static size_t resolve_column(int64_t i, workframe& wf) {
  const DataTable* dt = wf.get_datatable(0);
  int64_t incols = static_cast<int64_t>(dt->ncols);
  if (i < -incols || i >= incols) {
    throw ValueError() << "Column index `" << i << "` is invalid for a Frame "
        "with " << incols << " column" << (incols == 1? "" : "s");
  }
  if (i < 0) i += incols;
  return static_cast<size_t>(i);
}

// `name` must be a py::ostring
static size_t resolve_column(py::robj name, workframe& wf) {
  const DataTable* dt = wf.get_datatable(0);
  return dt->xcolindex(name);
}




//------------------------------------------------------------------------------
// allcols_jn
//------------------------------------------------------------------------------

/**
 * j_node representing selection of all columns (i.e. `:`). This is roughly
 * the equivalent of SQL's "*".
 *
 * In the simplest case, this node selects all columns from the source Frame.
 * The groupby field, if present, is ignored and the columns are selected as-is,
 * applying the RowIndex that was already computed. The names of the selected
 * columns will be exactly the same as in the source Frame.
 *
 * However, when 2 or more Frames are joined, this selector will select all
 * columns from all joined Frames. The exception to this are natural joins,
 * where the key columns of joined Frames will be excluded from the result.
 */
class allcols_jn : public j_node {
  public:
    allcols_jn() = default;
    DataTable* select(workframe&) override;
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
      names.insert(names.end(), dti_names.begin(), dti_names.end());
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




//------------------------------------------------------------------------------
// collist_jn
//------------------------------------------------------------------------------

class collist_jn : public j_node {
  private:
    std::vector<size_t> indices;
    strvec names;

  public:
    collist_jn(std::vector<size_t>&& cols);
    collist_jn(std::vector<size_t>&& cols, strvec&& names_);
    DataTable* select(workframe& wf) override;
};

collist_jn::collist_jn(std::vector<size_t>&& cols)
  : indices(std::move(cols)) {}

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


static collist_jn* _collist_from_slice(py::oslice src, workframe& wf) {
  size_t len = wf.get_datatable(0)->ncols;
  size_t start, count, step;
  src.normalize(len, &start, &count, &step);
  std::vector<size_t> indices;
  indices.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    indices.push_back(start + i * step);
  }
  return new collist_jn(std::move(indices));
}


// Note that the end in `start:end` is considered inclusive in a
// string slice
static collist_jn* _collist_from_string_slice(py::oslice src, workframe& wf) {
  const DataTable* dt = wf.get_datatable(0);
  py::oobj ostart = src.start_obj();
  py::oobj ostop = src.stop_obj();
  size_t start = ostart.is_none()? 0 : dt->xcolindex(ostart);
  size_t end = ostop.is_none()? dt->ncols - 1 : dt->xcolindex(ostop);
  size_t len = start <= end? end - start + 1 : start - end + 1;
  std::vector<size_t> indices;
  indices.reserve(len);
  if (start <= end) {
    for (size_t i = start; i <= end; ++i) {
      indices.push_back(i);
    }
  } else {
    // Careful with the case when `end = 0`. In this case a regular for-loop
    // `(i = start; i >= end; --i)` will become infinite.
    size_t i = start;
    do {
      indices.push_back(i);
    } while (i-- != end);
  }
  return new collist_jn(std::move(indices));
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
    exprlist_jn(exprvec&& cols);
    exprlist_jn(exprvec&& cols, strvec&& names_);
    DataTable* select(workframe& wf) override;
};

exprlist_jn::exprlist_jn(exprvec&& cols)
  : exprs(std::move(cols)) {}

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


static j_node* _from_base_expr(py::robj src, workframe& wf) {
  py::oobj res = src.invoke("_core");
  xassert(res.typeobj() == &py::base_expr::Type::type);
  auto pybe = reinterpret_cast<py::base_expr*>(res.to_borrowed_ref());
  auto expr = std::unique_ptr<dt::base_expr>(pybe->release());
  if (expr->is_primary_col_selector()) {
    size_t i = expr->get_primary_col_index(wf);
    return new collist_jn({ i });
  }
  exprvec exprs;
  exprs.push_back(std::move(expr));
  return new exprlist_jn(std::move(exprs));
}


enum list_type {
  UNKNOWN, BOOL, INT, STR, EXPR
};

static void _check_list_type(size_t k, list_type prev, list_type curr) {
  if (prev == curr) return;
  static const char* names[] = {"?", "boolean", "integer", "string", "expr"};
  throw TypeError() << "Mixed selector types in `j` are not allowed. "
      "Element " << k << " is of type " << names[curr] << ", whereas the "
      "previous element(s) were of type " << names[prev];
}


static j_node* _from_list(py::robj src, workframe& wf) {
  const DataTable* dt0 = wf.get_datatable(0);
  list_type type = UNKNOWN;
  std::vector<size_t> indices;
  std::vector<std::string> names;
  exprvec exprs;
  size_t k = 0;
  auto process_element = [&](py::robj elem) {
    if (elem.is_int()) {
      if (type == UNKNOWN) type = INT;
      _check_list_type(k, type, INT);
      int64_t i = elem.to_int64_strict();
      size_t j = resolve_column(i, wf);
      indices.push_back(j);
    }
    else if (elem.is_bool()) {
      if (type == UNKNOWN) type = BOOL;
      _check_list_type(k, type, BOOL);
      int t = elem.to_bool_strict();
      if (t) indices.push_back(k);
    }
    else if (elem.is_string()) {
      if (type == UNKNOWN) type = STR;
      _check_list_type(k, type, STR);
      size_t j = dt0->xcolindex(elem);
      indices.push_back(j);
    }
    else if (is_PyBaseExpr(elem)) {
      if (type == UNKNOWN) type = EXPR;
      _check_list_type(k, type, EXPR);
      py::oobj res = elem.invoke("_core");
      xassert(res.typeobj() == &py::base_expr::Type::type);
      auto pybe = reinterpret_cast<py::base_expr*>(res.to_borrowed_ref());
      auto expr = std::unique_ptr<dt::base_expr>(pybe->release());
      if (expr->is_primary_col_selector()) {
        size_t i = expr->get_primary_col_index(wf);
        indices.push_back(i);
      }
      exprs.push_back(std::move(expr));
    }
    else {
      throw TypeError() << "Element " << k << " in the `j` selector list has "
          "type `" << elem.typeobj() << "`, which is not a proper column "
          "selector";
    }
    ++k;
  };

  if (src.is_list_or_tuple()) {
    py::olist srclist = src.to_pylist();
    size_t nelems = srclist.size();
    for (size_t i = 0; i < nelems; ++i) {
      process_element(srclist[i]);
    }
  } else if (src.is_dict()) {
    py::odict srcdict = src.to_pydict();
    type = EXPR;
    for (auto kv : srcdict) {
      if (!kv.first.is_string()) {
        throw TypeError() << "Keys in `j` selector dictionary must be strings";
      }
      names.push_back(kv.first.to_string());
      process_element(kv.second);
    }
  } else {
    for (auto elem : src.to_oiter()) {
      process_element(elem);
    }
  }

  if (type == BOOL && k != dt0->ncols) {
    throw ValueError() << "The length of boolean list `j` does not match the "
        "number of columns in the Frame: " << k << " vs " << dt0->ncols;
  }
  // A list of "EXPR" type may be either a list of plain column selectors
  // (such as `f.A`), or a list of more complicated expressions. In the former
  // case the vector of `indices` will be the same size as `exprs`, and we
  // return a `collist_jn` node. In the latter case, the `exprlist_jn` node is
  // created.
  if (exprs.size() > indices.size()) {
    xassert(type == EXPR);
    return new exprlist_jn(std::move(exprs), std::move(names));
  }
  return new collist_jn(std::move(indices), std::move(names));
}




//------------------------------------------------------------------------------
// j_node factory function
//------------------------------------------------------------------------------

static j_node* _make(py::robj src, workframe& wf) {
  // The most common case is `:`, a trivial slice
  if (src.is_slice()) {
    auto ssrc = src.to_oslice();
    if (ssrc.is_trivial()) return new allcols_jn();
    if (ssrc.is_numeric()) {
      return _collist_from_slice(ssrc, wf);
    }
    if (ssrc.is_string()) {
      return _collist_from_string_slice(ssrc, wf);
    }
    throw TypeError() << src << " is neither integer- nor string-valued";
  }
  if (is_PyBaseExpr(src)) {
    return _from_base_expr(src, wf);
  }
  if (src.is_int()) {
    size_t i = resolve_column(src.to_int64_strict(), wf);
    return new collist_jn({ i });
  }
  if (src.is_string()) {
    size_t i = resolve_column(src, wf);
    return new collist_jn({ i });
  }
  if (src.is_list_or_tuple() || src.is_dict()) {
    return _from_list(src, wf);
  }
  if (src.is_dict()) {
    return nullptr;
  }
  if (src.is_iterable()) {
    return _from_list(src, wf);
  }
  if (src.is_none() || src.is_ellipsis()) {
    return new allcols_jn();
  }
  if (src.is_bool()) {
    throw TypeError() << "Boolean value cannot be used as a `j` expression";
  }
  return nullptr;  // for now
}

jptr j_node::make(py::robj src, workframe& wf) {
  return jptr(_make(src, wf));
}


j_node::~j_node() {}

void j_node::delete_(workframe&) {}

void j_node::update(workframe&) {}




/*******************************

    if isinstance(arg, dict):
        isarray = True
        outcols = []
        colnames = []
        for name, col in arg.items():
            pcol = process_column(col, dt)
            colnames.append(name)
            if isinstance(pcol, int):
                outcols.append(pcol)
            elif isinstance(pcol, tuple):
                start, count, step = pcol
                for i in range(count):
                    j = start + i * step
                    outcols.append(j)
                    if i > 0:
                        colnames.append(name + str(i))
            else:
                isarray = False
                outcols.append(pcol)
        if isarray:
            return ArrayCSNode(ee, outcols, colnames)
        else:
            return MixedCSNode(ee, outcols, colnames)

    if isinstance(arg, (type, ltype)):
        ltypes = dt.ltypes
        lt = ltype(arg)
        outcols = []
        colnames = []
        for i in range(dt.ncols):
            if ltypes[i] == lt:
                outcols.append(i)
                colnames.append(dt.names[i])
        return ArrayCSNode(ee, outcols, colnames)

    if isinstance(arg, stype):
        stypes = dt.stypes
        outcols = []
        colnames = []
        for i in range(dt.ncols):
            if stypes[i] == arg:
                outcols.append(i)
                colnames.append(dt.names[i])
        return ArrayCSNode(ee, outcols, colnames)

    raise TValueError("Unknown `select` argument: %r" % arg)


def process_column(col, df, new_cols_allowed=False):

    if isinstance(col, slice):
        start = col.start
        stop = col.stop
        step = col.step
        if isinstance(start, str) or isinstance(stop, str):
            col0 = None
            col1 = None
            if start is None:
                col0 = 0
            elif isinstance(start, str):
                col0 = df.colindex(start)
            if stop is None:
                col1 = df.ncols - 1
            elif isinstance(stop, str):
                col1 = df.colindex(stop)
            if col0 is None or col1 is None:
                raise TValueError("Slice %r is invalid: cannot mix numeric and "
                                  "string column names" % col)
            if step is not None:
                raise TValueError("Column name slices cannot use strides: %r"
                                  % col)
            return (col0, abs(col1 - col0) + 1, 1 if col1 >= col0 else -1)
        elif all(x is None or isinstance(x, int) for x in (start, stop, step)):
            return normalize_slice(col, df.ncols)
        else:
            raise TValueError("%r is not integer-valued" % col)


 *******************************/



}  // namespace dt
