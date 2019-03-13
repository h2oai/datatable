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
#include <iostream>
#include "frame/py_frame.h"
#include "models/dt_ftrl.h"
#include "models/column_convertor.h"
#include "models/utils.h"
#include "python/_all.h"
#include "python/obj.h"
#include "utils/parallel.h"
#include "utils/shared_mutex.h"
#include "datatablemodule.h"
#include "options.h"
#include "rowindex.h"

// Number of steps for the aggregator progress bar
#define PBSTEPS 100

// Define templated types for Aggregator
template <typename T>
using ccptr = typename std::unique_ptr<ColumnConvertor<T>>;
template <typename T>
using ccptrvec = typename std::vector<ccptr<T>>;


/*
*  Aggregator base class.
*/
class AggregatorBase {
  public :
    virtual void aggregate(DataTable*, dtptr&, dtptr&) = 0;
    virtual ~AggregatorBase();
};


/*
*  Aggregator main template class, where `T` is a type used for all the distance
*  calculations. Templated with either `float` or `double`, aggregator
*  converges to roughly the same number of exemplars, and members distribution.
*  At the same time, using `float` can reduce memory usage.
*/
template <typename T>
class Aggregator : public AggregatorBase {
  public:
    struct exemplar {
      size_t id;
      tptr<T> coords;
    };
    using exptr = std::unique_ptr<exemplar>;
    Aggregator(size_t, size_t, size_t, size_t, size_t, size_t,
               unsigned int, py::oobj, unsigned int);
    void aggregate(DataTable*, dtptr&, dtptr&) override;
    static constexpr T epsilon = std::numeric_limits<T>::epsilon();
    static void set_norm_coeffs(T&, T&, T, T, size_t);

  private:
    // Input parameters and datatable
    const DataTable* dt;
    size_t min_rows;
    size_t n_bins;
    size_t nx_bins;
    size_t ny_bins;
    size_t nd_max_bins;
    size_t max_dimensions;
    unsigned int seed;
    unsigned int nthreads;
    py::oobj progress_fn;

    // Output exemplar and member datatables
    dtptr dt_exemplars;
    dtptr dt_members;

    // Continuous column convertors and datatable with categorical columns
    ccptrvec<T> contconvs;
    dtptr dt_cat;

    // Final aggregation method
    void aggregate_exemplars(bool);

    // Grouping methods, `0d` means no grouping is done
    void group_0d();
    void group_1d();
    void group_1d_continuous();
    void group_1d_categorical();
    void group_2d();
    void group_2d_continuous();
    void group_2d_categorical();
    template<typename U0, typename U1>
    void group_2d_categorical_str();
    void group_2d_mixed();
    template<typename U0>
    void group_2d_mixed_str();
    void group_nd();

    // Random sampling and modular quasi-random generator
    bool sample_exemplars(size_t, size_t);

    // Helper methods
    size_t get_nthreads(size_t nrows);
    void normalize_row(tptr<T>&, size_t);
    void project_row(tptr<T>&, size_t, tptr<T>&);
    tptr<T> generate_pmatrix(size_t ncols);
    T calculate_distance(tptr<T>&, tptr<T>&, size_t, T, bool early_exit = true);
    void adjust_delta(T&, std::vector<exptr>&, std::vector<size_t>&, size_t);
    void adjust_members(std::vector<size_t>&);
    size_t calculate_map(std::vector<size_t>&, size_t);
    void progress(float, int status_code=0);
};


extern template class Aggregator<float>;
extern template class Aggregator<double>;
