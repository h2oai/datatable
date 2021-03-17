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
#ifndef dt_MODELS_LINEARMODEL_TYPES_h
#define dt_MODELS_LINEARMODEL_TYPES_h


namespace dt {


/**
 *  Supported linear model types.
 */
enum class LinearModelType : size_t {
  UNKNOWN     = 0, // Unknown model type
  AUTO        = 1, // Automatically detect model type
  REGRESSION  = 2, // Numerical regression
  BINOMIAL    = 3, // Binomial logistic regression
  MULTINOMIAL = 4  // Multinomial logistic regression
};


/**
 *  LinearModel parameters and their default values.
 */
struct LinearModelParams {
  LinearModelType model_type;
  double eta;
  double lambda1;
  double lambda2;
  double nepochs;
  bool double_precision;
  bool negative_class;
  size_t : 48;
  LinearModelParams() : model_type(LinearModelType::AUTO),
                 eta(0.005), lambda1(0.0), lambda2(0.0),
                 nepochs(1.0), double_precision(false),
                 negative_class(false)
                 {}
};


/**
 *  When linear model fitting is completed, this structure is returned
 *  containing epoch at which fitting stopped and, in the case validation set
 *  was provided, the corresponding final loss.
 */
struct LinearModelFitOutput {
  double epoch;
  double loss;
  LinearModelFitOutput() {
    epoch = std::numeric_limits<double>::quiet_NaN();
    loss = std::numeric_limits<double>::quiet_NaN();
  }
  LinearModelFitOutput(double epoch_, double loss_) {
    epoch = epoch_;
    loss = loss_;
  }
};


} // namespace dt

#endif
