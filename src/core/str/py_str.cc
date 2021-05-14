//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "str/py_str.h"
#include "datatablemodule.h"
#include "frame/py_frame.h"
#include "stype.h"
#include "utils/exceptions.h"
namespace py {


static const char* doc_split_into_nhot =
R"(split_into_nhot(frame, sep=",", sort=False)
--

Split and nhot-encode a single-column frame.

Each value in the frame, having a single string column, is split according
to the provided separator `sep`, the whitespace is trimmed, and
the resulting pieces (labels) are converted into the individual columns
of the output frame.


Parameters
----------
frame: Frame
    An input single-column frame. The column stype must be either `str32`
    or `str64`.

sep: str
    Single-character separator to be used for splitting.

sort: bool
    An option to control whether the resulting column names, i.e. labels,
    should be sorted. If set to `True`, the column names are returned in
    alphabetical order, otherwise their order is not guaranteed
    due to the algorithm parallelization.

return: Frame
    The output frame. It will have as many rows as the input frame,
    and as many boolean columns as there were unique labels found.
    The labels will also become the output column names.

except: ValueError | TypeError
    :exc:`dt.exceptions.ValueError`
        Raised if the input frame is missing or it has more
        than one column. It is also raised if `sep` is not a single-character
        string.

    :exc:`dt.exceptions.TypeError`
        Raised if the single column of `frame` has non-string stype.


Examples
--------

.. code-block:: python

    >>> DT = dt.Frame(["cat,dog", "mouse", "cat,mouse", "dog,rooster", "mouse,dog,cat"])
    >>> DT
       | C0
       | str32
    -- + -------------
     0 | cat,dog
     1 | mouse
     2 | cat,mouse
     3 | dog,rooster
     4 | mouse,dog,cat
    [5 rows x 1 column]

    >>> dt.split_into_nhot(DT)
       |   cat    dog  mouse  rooster
       | bool8  bool8  bool8    bool8
    -- + -----  -----  -----  -------
     0 |     1      1      0        0
     1 |     0      0      1        0
     2 |     1      0      1        0
     3 |     0      1      0        1
     4 |     1      1      1        0
    [5 rows x 4 columns]
)";



static PKArgs args_split_into_nhot(
    1, 0, 2, false, false,
    {"frame", "sep", "sort"}, "split_into_nhot", doc_split_into_nhot
);


static oobj split_into_nhot(const PKArgs& args) {
  if (args[0].is_undefined()) {
    throw ValueError() << "Required parameter `frame` is missing";
  }

  if (args[0].is_none()) {
    return py::None();
  }

  DataTable* dt = args[0].to_datatable();
  std::string sep = args[1]? args[1].to_string() : ",";
  bool sort = args[2]? args[2].to_bool_strict() : false;

  if (dt->ncols() != 1) {
    throw ValueError() << "Function split_into_nhot() may only be applied to "
      "a single-column Frame of type string;" << " got frame with "
      << dt->ncols() << " columns";
  }
  const Column& col0 = dt->get_column(0);
  dt::SType st = col0.stype();
  if (!(st == dt::SType::STR32 || st == dt::SType::STR64)) {
    throw TypeError() << "Function split_into_nhot() may only be applied to "
      "a single-column Frame of type string;" << " received a column of type "
      << st;
  }
  if (sep.size() != 1) {
    throw ValueError() << "Parameter `sep` in split_into_nhot() must be a "
      "single character; got '" << sep << "'";
  }

  DataTable* res = dt::split_into_nhot(col0, sep[0], sort);
  return Frame::oframe(res);
}


void DatatableModule::init_methods_str() {
  ADD_FN(&split_into_nhot, args_split_into_nhot);
}

} // namespace py
