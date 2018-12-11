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
#include "expr/i_node.h"
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/string.h"
namespace dt {



//------------------------------------------------------------------------------
// allrows_in
//------------------------------------------------------------------------------

/**
 * i_node representing selection of all rows from a Frame.
 *
 * Although "all rows" selector can easily be implemented as a slice, we want
 * to have a separate class because (1) this is a very common selector type,
 * and (2) in some cases useful optimizations can be achieved if we know that
 * all rows were selected.
 */
class allrows_in : public i_node {
  public:
    allrows_in() = default;
    void execute(workframe&) override;
};


// All rows are selected, so no need to change the workframe.
void allrows_in::execute(workframe&) {}




//------------------------------------------------------------------------------
// onerow_in
//------------------------------------------------------------------------------

class onerow_in : public i_node {
  private:
    int64_t irow;

  public:
    onerow_in(int64_t i);
    void post_init_check(workframe&) override;
    void execute(workframe&) override;
};


onerow_in::onerow_in(int64_t i) : irow(i) {}


void onerow_in::post_init_check(workframe& wf) {
  int64_t inrows = static_cast<int64_t>(wf.nrows());
  if (irow < -inrows || irow >= inrows) {
    throw ValueError() << "Row `" << irow << "` is invalid for a frame with "
        << inrows << " row" << (inrows == 1? "" : "s");
  }
  if (irow < 0) irow += inrows;
}


void onerow_in::execute(workframe& wf) {
  size_t start = static_cast<size_t>(irow);
  wf.apply_rowindex(RowIndex(start, 1, 1));
}




//------------------------------------------------------------------------------
// slice_in
//------------------------------------------------------------------------------

class slice_in : public i_node {
  private:
    int64_t istart, istop, istep;
    bool is_slice;
    size_t : 56;

  public:
    slice_in(int64_t, int64_t, int64_t, bool);
    void execute(workframe&) override;
};


slice_in::slice_in(int64_t _start, int64_t _stop, int64_t _step, bool _slice)
{
  istart = _start;
  istop = _stop;
  istep = _step;
  is_slice = _slice;
}


void slice_in::execute(workframe& wf) {
  size_t nrows = wf.nrows();
  size_t start, count, step;
  if (is_slice) {
    py::oslice::normalize(nrows, istart, istop, istep, &start, &count, &step);
  }
  else {
    bool ok = py::orange::normalize(nrows, istart, istop, istep,
                                    &start, &count, &step);
    if (!ok) {
      throw ValueError() << "range(" << istart << ", " << istop << ", "
          << istep << ") cannot be applied to a Frame with " << nrows
          << " row" << (nrows == 1? "" : "s");
    }
  }
  wf.apply_rowindex(RowIndex(start, count, step));
}




//------------------------------------------------------------------------------
// expr_in
//------------------------------------------------------------------------------

class expr_in : public i_node {
  private:
    dt::base_expr* expr;

  public:
    explicit expr_in(py::robj src);
    ~expr_in() override;
    void execute(workframe&) override;
};


expr_in::expr_in(py::robj src) {
  expr = nullptr;
  py::oobj res = src.invoke("_core");
  xassert(res.typeobj() == &py::base_expr::Type::type);
  auto pybe = reinterpret_cast<py::base_expr*>(res.to_borrowed_ref());
  expr = pybe->release();
}


expr_in::~expr_in() {
  delete expr;
}


void expr_in::execute(workframe& wf) {
  SType st = expr->resolve(wf);
  if (st != SType::BOOL) {
    throw TypeError() << "Filter expression must be of `bool8` type, instead it "
        "was of type " << st;
  }
  Column* col = expr->evaluate_eager(wf);
  RowIndex res(col);
  wf.apply_rowindex(res);
  delete col;
}



//------------------------------------------------------------------------------
// frame_in
//------------------------------------------------------------------------------

class frame_in : public i_node {
  private:
    const DataTable* dt;  // borrowed ref

  public:
    explicit frame_in(py::robj src);
    void post_init_check(workframe&) override;
    void execute(workframe&) override;
};


frame_in::frame_in(py::robj src) {
  dt = src.to_frame();
  if (dt->ncols != 1) {
    throw ValueError() << "Only a single-column Frame may be used as `i` "
        "selector, instead got a Frame with " << dt->ncols << " columns";
  }
  SType st = dt->columns[0]->stype();
  if (!(st == SType::BOOL || info(st).ltype() == LType::INT)) {
    throw TypeError() << "A Frame which is used as an `i` selector should be "
        "either boolean or integer, instead got `" << st << "`";
  }
}


void frame_in::post_init_check(workframe& wf) {
  Column* col = dt->columns[0];
  size_t nrows = wf.nrows();
  if (col->stype() == SType::BOOL) {
    if (col->nrows != nrows) {
      throw ValueError() << "A boolean column used as `i` selector has "
          << col->nrows << " row" << (col->nrows == 1? "" : "s")
          << ", but applied to a Frame with "
           << nrows << " row" << (nrows == 1? "" : "s");
    }
  } else {
    int64_t min = col->min_int64();
    int64_t max = col->max_int64();
    if (min < -1) {
      throw ValueError() << "An integer column used as an `i` selector "
          "contains invalid negative indices: " << min;
    }
    if (max >= static_cast<int64_t>(nrows)) {
      throw ValueError() << "An integer column used as an `i` selector "
          "contains index " << max << " which is not valid for a Frame with "
          << nrows << " row" << (nrows == 1? "" : "s");
    }
  }
}


void frame_in::execute(workframe& wf) {
  RowIndex ri { dt->columns[0] };
  wf.apply_rowindex(ri);
}



//------------------------------------------------------------------------------
// nparray
//------------------------------------------------------------------------------

static i_node* _from_nparray(py::oobj src) {
  py::otuple shape = src.get_attr("shape").to_otuple();
  size_t ndims = shape.size();
  if (ndims == 2) {
    size_t dim0 = shape[0].to_size_t();
    size_t dim1 = shape[1].to_size_t();
    if (dim0 == 1 || dim1 == 1) {
      py::otuple args(1);
      args.set(0, py::oint(dim0 * dim1));
      src = src.invoke("reshape", args);
      shape = src.get_attr("shape").to_otuple();
      ndims = shape.size();
    }
  }
  if (ndims != 1) {
    throw ValueError() << "Only a single-dimensional numpy array is allowed "
        "as `i` selector, got array of shape " << shape;
  }
  py::ostring dtype = src.get_attr("dtype").to_pystring_force();
  std::string dtype_str { PyUnicode_AsUTF8(dtype.to_borrowed_ref()) };
  bool is_bool = dtype_str.compare(0, 4, "bool");
  bool is_int = dtype_str.compare(0, 3, "int");
  if (!(is_bool || is_int)) {
    throw TypeError() << "Either a boolean or an integer numpy array expected "
        "for an `i` selector, got array of dtype `" << dtype_str << "`";
  }
  // Now convert numpy array into a datatable Frame
  auto dt_Frame = py::oobj(reinterpret_cast<PyObject*>(&py::Frame::Type::type));
  py::otuple args(1);
  args.set(0, src);
  py::oobj frame = dt_Frame.call(args);
  return new frame_in(frame);
}




//------------------------------------------------------------------------------
// i_node
//------------------------------------------------------------------------------

i_node::~i_node() {}

void i_node::post_init_check(workframe&) {}


i_node* i_node::make(py::robj src) {
  // The most common case is `:`, a trivial slice
  if (src.is_slice()) {
    auto ssrc = src.to_oslice();
    if (ssrc.is_trivial()) return new allrows_in();
    if (ssrc.is_numeric()) {
      return new slice_in(ssrc.start(), ssrc.stop(), ssrc.step(), true);
    }
    throw TypeError() << src << " is not integer-valued";
  }
  if (is_PyBaseExpr(src)) {
    return new expr_in(src);
  }
  if (src.is_frame()) {
    return new frame_in(src);
  }
  if (src.is_int()) {
    int64_t val = src.to_int64_strict();
    return new onerow_in(val);
  }
  if (src.is_none() || src.is_ellipsis()) {
    return new allrows_in();
  }
  if (src.is_numpy_array()) {
    return _from_nparray(src);
  }
  if (src.is_range()) {
    auto ss = src.to_orange();
    return new slice_in(ss.start(), ss.stop(), ss.step(), false);
  }
  // "iterable" is a very generic interface, so it must come close to last
  // in the resolution sequence
  if (src.is_iterable()) {

  }
  if (src.is_bool()) {
    throw TypeError() << "Boolean value cannot be used as an `i` expression";
  }
  return nullptr;  // for now
}





/*******

    if isinstance(rows, (list, tuple, set)):
        bases = []
        counts = []
        steps = []
        for i, elem in enumerate(rows):
            if isinstance(elem, int):
                if -nrows <= elem < nrows:
                    # `elem % nrows` forces the row number to become positive
                    bases.append(elem % nrows)
                else:
                    raise TValueError(
                        "Row `%d` is invalid for datatable with %s"
                        % (elem, plural(nrows, "row")))
            elif isinstance(elem, (range, slice)):
                if not all(x is None or isinstance(x, int)
                           for x in (elem.start, elem.stop, elem.step)):
                    raise TValueError("%r is not integer-valued" % elem)
                if isinstance(elem, range):
                    res = normalize_range(elem, nrows)
                    if res is None:
                        raise TValueError(
                            "Invalid %r for a datatable with %s"
                            % (elem, plural(nrows, "row")))
                else:
                    res = normalize_slice(elem, nrows)
                start, count, step = res
                assert count >= 0
                if count == 0:
                    pass  # don't do anything
                elif count == 1:
                    bases.append(start)
                else:
                    if len(counts) < len(bases):
                        counts += [1] * (len(bases) - len(counts))
                        steps += [1] * (len(bases) - len(steps))
                    bases.append(start)
                    counts.append(count)
                    steps.append(step)
            else:
                if from_generator:
                    raise TValueError(
                        "Invalid row selector %r generated at position %d"
                        % (elem, i))
                else:
                    raise TValueError(
                        "Invalid row selector %r at element %d of the "
                        "`rows` list" % (elem, i))
        if not counts:
            if len(bases) == 1:
                if bases[0] == 0 and nrows == 1:
                    return AllRFNode(ee)
                return SliceRFNode(ee, bases[0], 1, 1)
            else:
                return ArrayRFNode(ee, bases)
        elif len(bases) == 1:
            if bases[0] == 0 and counts[0] == nrows and steps[0] == 1:
                return AllRFNode(ee)
            else:
                return SliceRFNode(ee, bases[0], counts[0], steps[0])
        else:
            return MultiSliceRFNode(ee, bases, counts, steps)

 *******/



}  // namespace dt
