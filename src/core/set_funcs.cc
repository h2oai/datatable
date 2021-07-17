//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include <functional>
#include <tuple>
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/args.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "datatable.h"
#include "datatablemodule.h"
#include "sort.h"
#include "stype.h"
namespace dt {
namespace set {


struct named_colvec {
  colvec columns;
  std::string name;
};

struct sort_result {
  sztvec sizes;
  Column column;
  std::string colname;
  RowIndex ri;
  Groupby gb;
};


//------------------------------------------------------------------------------
// helper functions
//------------------------------------------------------------------------------

static py::oobj make_pyframe(sort_result&& sorted, Buffer&& buf) {
  // The array of rowindices `arr` is typically shuffled because the values
  // in the input are sorted before they are compared.
  RowIndex out_ri = RowIndex(std::move(buf), RowIndex::ARR32);
  sorted.column.apply_rowindex(out_ri);
  DataTable* dt = new DataTable({std::move(sorted.column)},
                                {std::move(sorted.colname)});
  return py::Frame::oframe(dt);
}


static named_colvec columns_from_args(const py::PKArgs& args) {
  using FN = std::function<void(py::robj,size_t)>;
  named_colvec result;

  FN process_arg = [&](py::robj arg, size_t level) {
    if (arg.is_frame()) {
      DataTable* dt = arg.to_datatable();
      if (dt->ncols() == 0) return;
      if (dt->ncols() > 1) {
        throw ValueError() << "Only single-column Frames are allowed, "
            "but received a Frame with " << dt->ncols() << " columns";
      }
      Column col = dt->get_column(0);  // copy
      col.materialize();
      result.columns.push_back(std::move(col));
      if (result.name.empty()) {
        result.name = dt->get_names()[0];
      }
    }
    else if (arg.is_iterable() && !arg.is_string() && level < 2) {
      for (auto item : arg.to_oiter()) {
        process_arg(item, level+1);
      }
    }
    else {
      throw TypeError() << args.get_short_name() << "() expects a list or "
          "sequence of Frames, but got an argument of type " << arg.typeobj();
    }
  };

  for (auto va : args.varargs()) {
    process_arg(va, 0);
  }
  return result;
}


static sort_result sort_columns(named_colvec&& ncv) {
  xassert(!ncv.columns.empty());
  sort_result res;
  res.colname = std::move(ncv.name);
  size_t cumsize = 0;
  for (const auto& col : ncv.columns) {
    cumsize += col.nrows();
    res.sizes.push_back(cumsize);
  }
  if (ncv.columns.size() == 1) {
    res.column = std::move(ncv.columns[0]);
    res.column.materialize();
  } else {
    res.column = Column::new_na_column(0, SType::VOID);
    res.column.rbind(ncv.columns, false);
  }
  auto r = group({res.column}, {SortFlag::NONE});
  res.ri = std::move(r.first);
  res.gb = std::move(r.second);
  return res;
}


static py::oobj _union(named_colvec&& ncv) {
  if (ncv.columns.empty()) {
    return py::Frame::oframe(new DataTable());
  }
  sort_result sorted = sort_columns(std::move(ncv));

  size_t ngrps = sorted.gb.size();
  const int32_t* goffsets = sorted.gb.offsets_r();
  if (goffsets[ngrps] == 0) ngrps = 0;

  const int32_t* indices = sorted.ri.indices32();
  Buffer buf = Buffer::mem(ngrps * sizeof(int32_t));
  int32_t* out_indices = static_cast<int32_t*>(buf.xptr());

  for (size_t i = 0; i < ngrps; ++i) {
    out_indices[i] = indices[goffsets[i]];
  }
  return make_pyframe(std::move(sorted), std::move(buf));
}



//------------------------------------------------------------------------------
// unique()
//------------------------------------------------------------------------------

static const char* doc_unique =
R"(unique(frame)
--

Find the unique values in all the columns of the `frame`.

This function sorts the values in order to find the uniques, so the return
values will be ordered. However, this should be considered an implementation
detail: in the future datatable may switch to a different algorithm,
such as hash-based, which may return the results in a different order.


Parameters
----------
frame: Frame
    Input frame.

return: Frame
    A single-column frame consisting of unique values found in `frame`.
    The column stype is the smallest common stype for all the
    `frame` columns.

except: NotImplementedError
    The exception is raised when one of the frame columns has stype `obj64`.


Examples
--------
.. code-block:: python

    >>> from datatable import dt
    >>>
    >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
    ...                'B': [None, 2, 3,4, 5],
    ...                'C': [1, 2, 1, 1, 2]})
    >>> df
       |     A      B      C
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1     NA      1
     1 |     1      2      2
     2 |     2      3      1
     3 |     1      4      1
     4 |     2      5      2
    [5 rows x 3 columns]

Unique values in the entire frame::

    >>> dt.unique(df)
       |    C0
       | int32
    -- + -----
     0 |    NA
     1 |     1
     2 |     2
     3 |     3
     4 |     4
     5 |     5
    [6 rows x 1 column]

Unique values in a frame with a single column::

    >>> dt.unique(df["A"])
       |     A
       | int32
    -- + -----
     0 |     1
     1 |     2
    [2 rows x 1 column]


See Also
--------
- :func:`intersect()` -- calculate the set intersection of values in the frames.
- :func:`setdiff()` -- calculate the set difference between the frames.
- :func:`symdiff()` -- calculate the symmetric difference between the sets of values in the frames.
- :func:`union()` -- calculate the union of values in the frames.
)";

static py::PKArgs args_unique(
    1, 0, 0,        // Number of pos-only, pos/kw, and kw-only args
    false, false,   // varargs/varkws allowed?
    {"frame"},      // arg names
    "unique",       // function name
    doc_unique      // documentation
);


static py::oobj unique(const py::PKArgs& args) {
  if (!args[0]) {
    throw ValueError() << "Function `unique()` expects a Frame as a parameter";
  }
  DataTable* dt = args[0].to_datatable();

  named_colvec ncv;
  for (size_t i = 0; i < dt->ncols(); ++i) {
    ncv.columns.push_back(dt->get_column(i));
  }
  if (dt->ncols() == 1) {
    ncv.name = dt->get_names()[0];
  }
  return _union(std::move(ncv));
}



//------------------------------------------------------------------------------
// union()
//------------------------------------------------------------------------------

static const char* doc_union =
R"(union(*frames)
--

Find the union of values in all `frames`.

Each frame should have only a single column or be empty. The values in each
frame will be treated as a set, and this function will perform the
`union operation <https://en.wikipedia.org/wiki/Union_(set_theory)>`_
on these sets.

The `dt.union(*frames)` operation is equivalent to
`dt.unique(dt.rbind(*frames))`.

Parameters
----------
*frames: Frame, Frame, ...
    Input single-column frames.

return: Frame
    A single-column frame. The column stype is the smallest common
    stype of columns in the `frames`.

except: ValueError | NotImplementedError
    .. list-table::
        :widths: auto
        :class: api-table

        * - :exc:`dt.exceptions.ValueError`
          - raised when one of the input frames has more than one column.

        * - :exc:`dt.exceptions.NotImplementedError`
          - raised when one of the columns has stype `obj64`.


Examples
--------
.. code-block:: python

    >>> from datatable import dt
    >>>
    >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
    ...                'B': [None, 2, 3,4, 5],
    ...                'C': [1, 2, 1, 1, 2]})
    >>> df
       |     A      B      C
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1     NA      1
     1 |     1      2      2
     2 |     2      3      1
     3 |     1      4      1
     4 |     2      5      2
    [5 rows x 3 columns]

Union of all the columns in a frame::

    >>> dt.union(*df)
       |     A
       | int32
    -- + -----
     0 |    NA
     1 |     1
     2 |     2
     3 |     3
     4 |     4
     5 |     5
    [6 rows x 1 column]

Union of two frames::

    >>> dt.union(df["A"], df["C"])
       |     A
       | int32
    -- + -----
     0 |     1
     1 |     2
    [2 rows x 1 column]


See Also
--------
- :func:`intersect()` -- calculate the set intersection of values in the frames.
- :func:`setdiff()` -- calculate the set difference between the frames.
- :func:`symdiff()` -- calculate the symmetric difference between the sets of values in the frames.
- :func:`unique()` -- find unique values in a frame.
)";


static py::PKArgs args_union(
    0, 0, 0,
    true, false,
    {},
    "union",
    doc_union
);


static py::oobj union_(const py::PKArgs& args) {
  named_colvec cc = columns_from_args(args);
  return _union(std::move(cc));
}



//------------------------------------------------------------------------------
// intersect()
//------------------------------------------------------------------------------

template <bool TWO>
static py::oobj _intersect(named_colvec&& cv) {
  size_t K = cv.columns.size();
  sort_result sorted = sort_columns(std::move(cv));
  size_t ngrps = sorted.gb.size();
  const int32_t* goffsets = sorted.gb.offsets_r();
  if (goffsets[ngrps] == 0) ngrps = 0;

  const int32_t* indices = sorted.ri.indices32();
  Buffer buffer = Buffer::mem(ngrps * sizeof(int32_t));
  int32_t* out_indices = static_cast<int32_t*>(buffer.xptr());
  size_t j = 0;

  if (TWO) {
    // When intersecting only 2 vectors, it is enough to check whether the
    // first element in a group is < n1 (belongs to column 0), and the last is
    // >= n1 (belongs to column 1).
    xassert(K == 2);
    int32_t n1 = static_cast<int32_t>(sorted.sizes[0]);
    for (size_t i = 0; i < ngrps; ++i) {
      int32_t x = indices[goffsets[i]];
      if (x < n1) {
        int32_t y = indices[goffsets[i + 1] - 1];
        if (y >= n1) {
          out_indices[j++] = x;
        }
      }
    }
  } else {
    // When intersecting 3+ vectors, we scan the rowindex by group, and
    // for each element in a group we check whether it belongs to each
    // of the K input vectors.
    xassert(K > 2);
    int32_t iK = static_cast<int32_t>(K);
    int32_t off0, off1 = 0;
    for (size_t i = 1; i <= ngrps; ++i) {
      off0 = off1;
      off1 = goffsets[i];
      int32_t ii = off0;
      if (off0 + iK > off1) continue;
      for (size_t k = 0; k < K; ++k) {
        int32_t nk = static_cast<int32_t>(sorted.sizes[k]);
        if (indices[ii] >= nk) goto cont_outer_loop;
        while (ii < off1 && indices[ii] < nk) ++ii;
        if (ii == off1) {
          if (k == K - 1) {
            out_indices[j++] = indices[off0];
          }
          break;
        }
      }
      cont_outer_loop:;
    }
  }
  buffer.resize(j * sizeof(int32_t));
  return make_pyframe(std::move(sorted), std::move(buffer));
}


static const char* doc_intersect =
R"(intersect(*frames)
--

Find the intersection of sets of values in the `frames`.

Each frame should have only a single column or be empty.
The values in each frame will be treated as a set, and this function will
perform the
`intersection operation <https://en.wikipedia.org/wiki/Intersection_(set_theory)>`_
on these sets, returning those values that are present in each
of the provided `frames`.

Parameters
----------
*frames: Frame, Frame, ...
    Input single-column frames.

return: Frame
    A single-column frame. The column stype is the smallest common
    stype of columns in the `frames`.

except: ValueError | NotImplementedError
    .. list-table::
        :widths: auto
        :class: api-table

        * - :exc:`dt.exceptions.ValueError`
          - raised when one of the input frames has more than one column.

        * - :exc:`dt.exceptions.NotImplementedError`
          - raised when one of the columns has stype `obj64`.


Examples
--------
.. code-block:: python

    >>> from datatable import dt
    >>>
    >>> s1 = dt.Frame([4, 5, 6, 20, 42])
    >>> s2 = dt.Frame([1, 2, 3, 5, 42])
    >>>
    >>> s1
       |    C0
       | int32
    -- + -----
     0 |     4
     1 |     5
     2 |     6
     3 |    20
     4 |    42
    [5 rows x 1 column]

    >>> s2
       |    C0
       | int32
    -- + -----
     0 |     1
     1 |     2
     2 |     3
     3 |     5
     4 |    42
    [5 rows x 1 column]


Intersection of the two frames::

    >>> dt.intersect([s1, s2])
       |    C0
       | int32
    -- + -----
     0 |     5
     1 |    42
    [2 rows x 1 column]



See Also
--------
- :func:`setdiff()` -- calculate the set difference between the frames.
- :func:`symdiff()` -- calculate the symmetric difference between the sets of values in the frames.
- :func:`union()` -- calculate the union of values in the frames.
- :func:`unique()` -- find unique values in a frame.
)";

static py::PKArgs args_intersect(
    0, 0, 0,
    true, false,
    {},
    "intersect",
    doc_intersect
);


static py::oobj intersect(const py::PKArgs& args) {
  named_colvec cv = columns_from_args(args);
  if (cv.columns.size() <= 1) {
    return _union(std::move(cv));
  }
  if (cv.columns.size() == 2) {
    return _intersect<true>(std::move(cv));
  } else {
    return _intersect<false>(std::move(cv));
  }
}



//------------------------------------------------------------------------------
// setdiff()
//------------------------------------------------------------------------------

static py::oobj _setdiff(named_colvec&& cv) {
  xassert(cv.columns.size() >= 2);
  sort_result sorted = sort_columns(std::move(cv));
  size_t ngrps = sorted.gb.size();
  const int32_t* goffsets = sorted.gb.offsets_r();
  if (goffsets[ngrps] == 0) ngrps = 0;

  const int32_t* indices = sorted.ri.indices32();
  Buffer buffer = Buffer::mem(ngrps * sizeof(int32_t));
  int32_t* out_indices = static_cast<int32_t*>(buffer.xptr());
  size_t j = 0;

  int32_t n1 = static_cast<int32_t>(sorted.sizes[0]);
  for (size_t i = 0; i < ngrps; ++i) {
    int32_t x = indices[goffsets[i]];
    int32_t y = indices[goffsets[i + 1] - 1];
    if (x < n1 && y < n1) {
      out_indices[j++] = x;
    }
  }
  buffer.resize(j * sizeof(int32_t));
  return make_pyframe(std::move(sorted), std::move(buffer));
}


static const char* doc_setdiff =
R"(setdiff(frame0, *frames)
--

Find the set difference between `frame0` and the other `frames`.

Each frame should have only a single column or be empty.
The values in each frame will be treated as a set, and this function will
compute the
`set difference  <https://en.wikipedia.org/wiki/Complement_(set_theory)#Relative_complement>`_
between the `frame0` and the union of the other
frames, returning those values that are present in the `frame0`,
but not present in any of the `frames`.

Parameters
----------
frame0: Frame
    Input single-column frame.

*frames: Frame, Frame, ...
    Input single-column frames.

return: Frame
    A single-column frame. The column stype is the smallest common
    stype of columns from the `frames`.

except: ValueError | NotImplementedError
    .. list-table::
        :widths: auto
        :class: api-table

        * - :exc:`dt.exceptions.ValueError`
          - raised when one of the input frames, i.e. `frame0`
            or any one from the `frames`, has more than one column.

        * - :exc:`dt.exceptions.NotImplementedError`
          - raised when one of the columns has stype `obj64`.

Examples
--------
.. code-block:: python

    >>> from datatable import dt
    >>>
    >>> s1 = dt.Frame([4, 5, 6, 20, 42])
    >>> s2 = dt.Frame([1, 2, 3, 5, 42])
    >>>
    >>> s1
       |    C0
       | int32
    -- + -----
     0 |     4
     1 |     5
     2 |     6
     3 |    20
     4 |    42
    [5 rows x 1 column]

    >>> s2
       |    C0
       | int32
    -- + -----
     0 |     1
     1 |     2
     2 |     3
     3 |     5
     4 |    42
    [5 rows x 1 column]

Set difference of the two frames::

    >>> dt.setdiff(s1, s2)
       |    C0
       | int32
    -- + -----
     0 |     4
     1 |     6
     2 |    20
    [3 rows x 1 column]



See Also
--------
- :func:`intersect()` -- calculate the set intersection of values in the frames.
- :func:`symdiff()` -- calculate the symmetric difference between the sets of values in the frames.
- :func:`union()` -- calculate the union of values in the frames.
- :func:`unique()` -- find unique values in a frame.
)";

static py::PKArgs args_setdiff(
    0, 0, 0,
    true, false,
    {},
    "setdiff",
    doc_setdiff
);

static py::oobj setdiff(const py::PKArgs& args) {
  named_colvec cv = columns_from_args(args);
  if (cv.columns.size() <= 1) {
    return _union(std::move(cv));
  }
  return _setdiff(std::move(cv));
}



//------------------------------------------------------------------------------
// symdiff()
//------------------------------------------------------------------------------

template <bool TWO>
static py::oobj _symdiff(named_colvec&& cv) {
  size_t K = cv.columns.size();
  sort_result sr = sort_columns(std::move(cv));
  size_t ngrps = sr.gb.size();
  const int32_t* goffsets = sr.gb.offsets_r();
  if (goffsets[ngrps] == 0) ngrps = 0;

  const int32_t* indices = sr.ri.indices32();
  Buffer buffer = Buffer::mem(ngrps * sizeof(int32_t));
  int32_t* out_indices = static_cast<int32_t*>(buffer.xptr());
  size_t j = 0;

  if (TWO) {
    // When sym-diffing only 2 vectors, it is enough to check whether the
    // first element in a group belongs to the same column as the last element
    // in a group
    xassert(K == 2);
    int32_t n1 = static_cast<int32_t>(sr.sizes[0]);
    for (size_t i = 0; i < ngrps; ++i) {
      int32_t x = indices[goffsets[i]];
      int32_t y = indices[goffsets[i + 1] - 1];
      if ((x < n1) == (y < n1)) {
        out_indices[j++] = x;
      }
    }
  } else {
    // When sym-diffing 3+ vectors, we scan the rowindex by group, and
    // for each group count the number of columns that have elements in
    // that group.
    xassert(K > 2);
    int32_t off0, off1 = 0;
    for (size_t i = 1; i <= ngrps; ++i) {
      off0 = off1;
      off1 = goffsets[i];
      int32_t ii = off0;
      size_t kk = 0;  // number of columns whose elements are in this group
      for (size_t k = 0; k < K; ++k) {
        int32_t nk = static_cast<int32_t>(sr.sizes[k]);
        if (indices[ii] >= nk) continue;
        kk++;
        while (ii < off1 && indices[ii] < nk) ++ii;
        if (ii == off1) break;
      }
      if (kk & 1) {
        out_indices[j++] = indices[off0];
      }
    }
  }
  buffer.resize(j * sizeof(int32_t));
  return make_pyframe(std::move(sr), std::move(buffer));
}


static const char* doc_symdiff =
R"(symdiff(*frames)
--

Find the symmetric difference between the sets of values in all `frames`.

Each frame should have only a single column or be empty.
The values in each frame will be treated as a set, and this function will
perform the
`symmetric difference <https://en.wikipedia.org/wiki/Symmetric_difference>`_
operation on these sets.

The symmetric difference of two frames are those values that are present in
either of the frames, but not in the both. The symmetric difference of more than
two frames are those values that are present in an odd number of frames.

Parameters
----------
*frames: Frame, Frame, ...
    Input single-column frames.

return: Frame
    A single-column frame. The column stype is the smallest common
    stype of columns from the `frames`.

except: ValueError | NotImplementedError
    .. list-table::
        :widths: auto
        :class: api-table

        * - :exc:`dt.exceptions.ValueError`
          - raised when one of the input frames has more than one column.

        * - :exc:`dt.exceptions.NotImplementedError`
          - raised when one of the columns has stype `obj64`.

Examples
--------
.. code-block:: python

    >>> from datatable import dt
    >>>
    >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
    ...                'B': [None, 2, 3, 4, 5],
    ...                'C': [1, 2, 1, 1, 2]})
    >>> df
       |     A      B      C
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1     NA      1
     1 |     1      2      2
     2 |     2      3      1
     3 |     1      4      1
     4 |     2      5      2
    [5 rows x 3 columns]

Symmetric difference of all the columns in the entire frame; Note that each column is treated as a separate frame::

    >>> dt.symdiff(*df)
       |     A
       | int32
    -- + -----
     0 |    NA
     1 |     2
     2 |     3
     3 |     4
     4 |     5
    [5 rows x 1 column]


Symmetric difference between two frames::

    >>> dt.symdiff(df["A"], df["B"])
       |     A
       | int32
    -- + -----
     0 |    NA
     1 |     1
     2 |     3
     3 |     4
     4 |     5
    [5 rows x 1 column]


See Also
--------
- :func:`intersect()` -- calculate the set intersection of values in the frames.
- :func:`setdiff()` -- calculate the set difference between the frames.
- :func:`union()` -- calculate the union of values in the frames.
- :func:`unique()` -- find unique values in a frame.
)";

static py::PKArgs args_symdiff(
    0, 0, 0,
    true, false,
    {},
    "symdiff",
    doc_symdiff
);


static py::oobj symdiff(const py::PKArgs& args) {
  named_colvec cv = columns_from_args(args);
  if (cv.columns.size() <= 1) {
    return _union(std::move(cv));
  }
  if (cv.columns.size() == 2) {
    return _symdiff<true>(std::move(cv));
  } else {
    return _symdiff<false>(std::move(cv));
  }
}



} // namespace set
} // namespace dt


void py::DatatableModule::init_methods_sets() {
  ADD_FN(&dt::set::unique, dt::set::args_unique);
  ADD_FN(&dt::set::union_, dt::set::args_union);
  ADD_FN(&dt::set::intersect, dt::set::args_intersect);
  ADD_FN(&dt::set::setdiff, dt::set::args_setdiff);
  ADD_FN(&dt::set::symdiff, dt::set::args_symdiff);
}
