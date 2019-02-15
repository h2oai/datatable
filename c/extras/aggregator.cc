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
#include <random>
#include "extras/aggregator.h"


/*
*  Reading data from Python and passing it to the C++ aggregator.
*/
namespace py {
  static PKArgs aggregate(
    11, 0, 0, false, false,
    {"dt", "min_rows", "n_bins", "nx_bins", "ny_bins", "nd_max_bins",
     "max_dimensions", "seed", "progress_fn", "nthreads", "buffer_rows"}, "aggregate", "",
     [](const py::PKArgs& args) -> py::oobj {
       DataTable* dt = args[0].to_frame();

       size_t min_rows = args[1].to_size_t();
       size_t n_bins = args[2].to_size_t();
       size_t nx_bins = args[3].to_size_t();
       size_t ny_bins = args[4].to_size_t();
       size_t nd_max_bins = args[5].to_size_t();
       size_t max_dimensions = args[6].to_size_t();

       unsigned int seed = static_cast<unsigned int>(args[7].to_size_t());
       py::oobj progress_fn = args[8].is_none()? py::None() : py::oobj(args[8]);
       unsigned int nthreads = static_cast<unsigned int>(args[9].to_size_t());
       size_t buffer_rows = args[10].to_size_t();

       Aggregator<float> agg(min_rows, n_bins, nx_bins, ny_bins, nd_max_bins,
                      max_dimensions, seed, progress_fn, nthreads, buffer_rows);

       // dt changes in-place with a new column added to the end of it
       dtptr dt_members, dt_exemplars;
       agg.aggregate(dt, dt_exemplars, dt_members);
       py::oobj df_exemplars = py::oobj::from_new_reference(
                               py::Frame::from_datatable(dt_exemplars.release())
                             );
       py::oobj df_members = py::oobj::from_new_reference(
                               py::Frame::from_datatable(dt_members.release())
                             );
       py::olist list(2);
       list.set(0, df_exemplars);
       list.set(1, df_members);

       return std::move(list);
     }
  );
}


void DatatableModule::init_methods_aggregate() {
  ADDFN(py::aggregate);
}
