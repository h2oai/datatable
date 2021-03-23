//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#ifndef dt_MODELS_LINEARMODEL_BASE_h
#define dt_MODELS_LINEARMODEL_BASE_h
#include "_dt.h"
#include "models/dt_linearmodel_types.h"

namespace dt {


/**
 *  An abstract base class for all the linear models. It declares methods
 *  invoked by `py::LinearModel`.
 */
class LinearModelBase {
  public:
    virtual ~LinearModelBase();

    // Fit and predict
    virtual LinearModelFitOutput fit(const LinearModelParams*,
                                     const DataTable*, const DataTable*,
                                     const DataTable*, const DataTable*,
                                     double, double, size_t) = 0;
    virtual dtptr predict(const DataTable*) = 0;
    virtual bool is_fitted() = 0;

    // Getters
    virtual py::oobj get_labels() = 0;
    virtual py::oobj get_model() = 0;
    virtual size_t get_nfeatures() = 0;
    virtual size_t get_nlabels() = 0;

    // Setters
    virtual void set_labels(const DataTable&) = 0;
    virtual void set_model(const DataTable&) = 0;

    // Minimum number of rows a thread gets for fitting and predicting
    static constexpr size_t MIN_ROWS_PER_THREAD = 10000;
};


} // namespace dt

#endif
