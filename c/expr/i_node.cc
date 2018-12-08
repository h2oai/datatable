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
#include "expr/i_node.h"
#include "expr/workframe.h"   // dt::workframe
namespace dt {


//------------------------------------------------------------------------------
// inode_impl
//------------------------------------------------------------------------------

/**
 * Base class for all "Row Filter" nodes. A row filter node represents the
 * `i` part in a `DT[i, j, ...]` call.
 *
 * When executed, this class will compute a RowIndex and apply it to the
 * provided workframe `wf`.
 */
class inode_impl {
  public:
    virtual ~inode_impl();
    virtual void execute(workframe& wf) = 0;
};

inode_impl::~inode_impl() {}



//------------------------------------------------------------------------------
// allrows_ii
//------------------------------------------------------------------------------

/**
 * i_node representing selection of all rows from a Frame.
 *
 * Although "all rows" selector can easily be implemented as a slice, we want
 * to have a separate class because (1) this is a very common selector type,
 * and (2) in some cases useful optimizations can be achieved if we know that
 * all rows were selected.
 */
class allrows_ii : public inode_impl {
  public:
    allrows_ii() = default;
    virtual void execute(workframe&) override;
};

// All rows are selected, so no need to change the workframe.
void allrows_ii::execute(workframe&) {}



//------------------------------------------------------------------------------
// slice
//------------------------------------------------------------------------------

class slice_ii : public inode_impl {
  private:
    int64_t start, stop, step;

  public:
    slice_ii(int64_t, int64_t, int64_t);
    virtual void execute(workframe&) override;
};


slice_ii::slice_ii(int64_t _start, int64_t _stop, int64_t _step) {
  start = _start;
  stop = _stop;
  step = _step;
}


void slice_ii::execute(workframe& wf) {
  // wf.apply_rowindex(ri);
}




//------------------------------------------------------------------------------
// i_node factory function
//------------------------------------------------------------------------------

static inode_impl* _make_impl(py::robj src) {
  if (src.is_slice()) {
    auto ssrc = src.to_oslice();
    if (ssrc.is_trivial()) return new allrows_ii();
    if (ssrc.is_numeric()) {
      return new slice_ii(ssrc.start(), ssrc.stop(), ssrc.step());
    }
    throw TypeError() << src << " is not integer-valued";
  }
  if (src.is_ellipsis() || src.is_none()) {
    return new allrows_ii();
  }

}


i_node::i_node(py::robj src)
  : impl(_make_impl(src)) {}



/*******

    if rows is True or rows is False:
        # Note: True/False are integer objects in Python
        raise TTypeError("Boolean value cannot be used as a `rows` selector")

    if isinstance(rows, (int, slice, range)):
        rows = [rows]

    from_generator = False
    if isinstance(rows, types.GeneratorType):
        # If an iterator is given, materialize it first. Otherwise there
        # is no way to ensure that the produced indices are valid.
        rows = list(rows)
        from_generator = True

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

    if is_type(rows, NumpyArray_t):
        arr = rows
        if not (len(arr.shape) == 1 or
                len(arr.shape) == 2 and min(arr.shape) == 1):
            raise TValueError("Only a single-dimensional numpy.array is allowed"
                              " as a `rows` argument, got %r" % arr)
        if len(arr.shape) == 2 and arr.shape[1] > 1:
            arr = arr.T
        if not (str(arr.dtype) == "bool" or str(arr.dtype).startswith("int")):
            raise TValueError("Either a boolean or an integer numpy.array is "
                              "expected for `rows` argument, got %r" % arr)
        if str(arr.dtype) == "bool" and arr.shape[-1] != nrows:
            raise TValueError("Cannot apply a boolean numpy array of length "
                              "%d to a datatable with %s"
                              % (arr.shape[-1], plural(nrows, "row")))
        rows = datatable.Frame(arr)
        assert rows.ncols == 1
        assert rows.ltypes[0] == ltype.bool or rows.ltypes[0] == ltype.int

    if is_type(rows, Frame_t):
        if rows.ncols != 1:
            raise TValueError("`rows` argument should be a single-column "
                              "datatable, got %r" % rows)
        col0type = rows.ltypes[0]
        if col0type == ltype.bool:
            if rows.nrows != nrows:
                s1rows = plural(rows.nrows, "row")
                s2rows = plural(nrows, "row")
                raise TValueError("`rows` datatable has %s, but applied to a "
                                  "datatable with %s" % (s1rows, s2rows))
            return BooleanColumnRFNode(ee, rows)
        elif col0type == ltype.int:
            return IntegerColumnRFNode(ee, rows)
        else:
            raise TTypeError("`rows` datatable should be either a boolean or "
                             "an integer column, however it has type %s"
                             % col0type)

    if isinstance(rows, types.FunctionType):
        return make_rowfilter(rows(f), ee, _nested=True)

    if isinstance(rows, BaseExpr):
        return FilterExprRFNode(ee, rows)

    if _nested:
        raise TTypeError("Unexpected result produced by the `rows` "
                         "function: %r" % (rows, ))
    else:
        raise TTypeError("Unexpected `rows` argument: %r" % (rows, ))

 *******/



}  // namespace dt
