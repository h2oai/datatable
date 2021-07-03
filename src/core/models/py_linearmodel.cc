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
#include <vector>
#include "documentation.h"
#include "frame/py_frame.h"
#include "models/dt_linearmodel_classification.h"
#include "models/dt_linearmodel_regression.h"
#include "models/py_linearmodel.h"
#include "models/py_validator.h"
#include "models/utils.h"
#include "python/_all.h"
#include "python/string.h"


namespace py {

const size_t LinearModel::API_VERSION = 1;
const size_t LinearModel::N_PARAMS = 11;

/**
 *  Model type names and their corresponding dt::LinearModelType's
 */
static const std::unordered_map<std::string, dt::LinearModelType> LinearModelNameType {
   {"auto", dt::LinearModelType::AUTO},
   {"regression", dt::LinearModelType::REGRESSION},
   {"binomial", dt::LinearModelType::BINOMIAL},
   {"multinomial", dt::LinearModelType::MULTINOMIAL}
};


/**
 *  Learning rate schedules
 */
static const std::unordered_map<std::string, dt::LearningRateSchedule> LearningRateScheduleName {
   {"constant", dt::LearningRateSchedule::CONSTANT},
   {"time-based", dt::LearningRateSchedule::TIME_BASED},
   {"step-based", dt::LearningRateSchedule::STEP_BASED},
   {"exponential", dt::LearningRateSchedule::EXPONENTIAL}
};


/**
 *  LinearModel(...)
 *  Initialize LinearModel object with the provided parameters.
 */

static const char* doc___init__ =
R"(__init__(self,
eta0=0.005, eta_decay=0.0001, eta_drop_rate=10.0, eta_schedule='constant',
lambda1=0, lambda2=0, nepochs=1, double_precision=False, negative_class=False,
model_type='auto', seed=0, params=None)
--

Create a new :class:`LinearModel <datatable.models.LinearModel>` object.

Parameters
----------
eta0: float
    The initial learning rate, should be positive.

eta_decay: float
    Decay for the `"time-based"` and `"step-based"`
    learning rate schedules, should be non-negative.

eta_drop_rate: float
    Drop rate for the `"step-based"` learning rate schedule,
    should be positive.

eta_schedule: "constant" | "time-based" | "step-based" | "exponential"
    Learning rate schedule. When it is `"constant"` the learning rate
    `eta` is constant and equals to `eta0`. Otherwise,
    after each training iteration `eta` is updated as follows:

    - for `"time-based"` schedule as `eta0 / (1 + eta_decay * epoch)`;
    - for `"step-based"` schedule as `eta0 * eta_decay ^ floor((1 + epoch) / eta_drop_rate)`;
    - for `"exponential"` schedule as `eta0 / exp(eta_decay * epoch)`.

    By default, the size of the training iteration is one epoch, it becomes
    `nepochs_validation` when validation dataset is specified.

lambda1: float
    L1 regularization parameter, should be non-negative.

lambda2: float
    L2 regularization parameter, should be non-negative.

nepochs: float
    Number of training epochs, should be non-negative. When `nepochs`
    is an integer number, the model will train on all the data
    provided to :meth:`.fit` method `nepochs` times. If `nepochs`
    has a fractional part `{nepochs}`, the model will train on all
    the data `[nepochs]` times, i.e. the integer part of `nepochs`.
    Plus, it will also perform an additional training iteration
    on the `{nepochs}` fraction of data.

double_precision: bool
    An option to indicate whether double precision, i.e. `float64`,
    or single precision, i.e. `float32`, arithmetic should be used
    for computations. It is not guaranteed that setting
    `double_precision` to `True` will automatically improve
    the model accuracy. It will, however, roughly double the memory
    footprint of the `LinearModel` object.

negative_class: bool
    An option to indicate if a "negative" class should be created
    in the case of multinomial classification. For the "negative"
    class the model will train on all the negatives, and if
    a new label is encountered in the target column, its
    coefficients will be initialized to the current "negative" class coefficients.
    If `negative_class` is set to `False`, the initial coefficients
    become zeros.

model_type: "binomial" | "multinomial" | "regression" | "auto"
    The model type to be built. When this option is `"auto"`
    then the model type will be automatically chosen based on
    the target column `stype`.

seed: int
    Seed for the quasi-random number generator that is used for
    data shuffling when fitting the model, should be non-negative.
    If seed is zero, no shuffling is performed.

params: LinearModelParams
    Named tuple of the above parameters. One can pass either this tuple,
    or any combination of the individual parameters to the constructor,
    but not both at the same time.

except: ValueError
    The exception is raised if both the `params` and one of the
    individual model parameters are passed at the same time.

)";


static PKArgs args___init__(0, 1, LinearModel::N_PARAMS, false, false,
                                 {"params",
                                 "eta0", "eta_decay", "eta_drop_rate", "eta_schedule",
                                 "lambda1", "lambda2",
                                 "nepochs", "double_precision", "negative_class",
                                 "model_type",
                                 "seed"},
                                 "__init__", doc___init__);



void LinearModel::m__init__(const PKArgs& args) {
  size_t i = 0;

  const Arg& arg_params           = args[i++];
  const Arg& arg_eta0             = args[i++];
  const Arg& arg_eta_decay        = args[i++];
  const Arg& arg_eta_drop_rate    = args[i++];
  const Arg& arg_eta_schedule     = args[i++];
  const Arg& arg_lambda1          = args[i++];
  const Arg& arg_lambda2          = args[i++];
  const Arg& arg_nepochs          = args[i++];
  const Arg& arg_double_precision = args[i++];
  const Arg& arg_negative_class   = args[i++];
  const Arg& arg_model_type       = args[i++];
  const Arg& arg_seed             = args[i++];
  xassert(i == N_PARAMS + 1);


  bool defined_params           = !arg_params.is_none_or_undefined();
  bool defined_eta0             = !arg_eta0.is_none_or_undefined();
  bool defined_eta_decay        = !arg_eta_decay.is_none_or_undefined();
  bool defined_eta_drop_rate    = !arg_eta_drop_rate.is_none_or_undefined();
  bool defined_eta_schedule     = !arg_eta_schedule.is_none_or_undefined();
  bool defined_lambda1          = !arg_lambda1.is_none_or_undefined();
  bool defined_lambda2          = !arg_lambda2.is_none_or_undefined();
  bool defined_nepochs          = !arg_nepochs.is_none_or_undefined();
  bool defined_double_precision = !arg_double_precision.is_none_or_undefined();
  bool defined_negative_class   = !arg_negative_class.is_none_or_undefined();
  bool defined_model_type       = !arg_model_type.is_none_or_undefined();
  bool defined_seed             = !arg_seed.is_none_or_undefined();
  bool defined_individual_param = defined_eta0 ||
                                  defined_lambda1 || defined_lambda2 ||
                                  defined_nepochs || defined_double_precision ||
                                  defined_negative_class || defined_seed;

  init_params();

  if (defined_params) {
    if (defined_individual_param) {
      throw ValueError() << "You can either pass all the parameters with "
        << "`params` or any of the individual parameters with "
        << "`eta0`, `eta_decay`, `eta_drop_rate`, `eta_schedule`, "
        << "`lambda1`, `lambda2`, `nepochs`, "
        << "`double_precision`, `negative_class`, `model_type` or `seed` "
        << "to `LinearModel` constructor, but not both at the same time";
    }

    py::otuple py_params_in = arg_params.to_otuple();
    set_params_namedtuple(py_params_in);

  } else {
    if (defined_eta0) set_eta0(arg_eta0);
    if (defined_eta_decay) set_eta_decay(arg_eta_decay);
    if (defined_eta_drop_rate) set_eta_drop_rate(arg_eta_drop_rate);
    if (defined_eta_schedule) set_eta_schedule(arg_eta_schedule);
    if (defined_lambda1) set_lambda1(arg_lambda1);
    if (defined_lambda2) set_lambda2(arg_lambda2);
    if (defined_nepochs) set_nepochs(arg_nepochs);
    if (defined_double_precision) set_double_precision(arg_double_precision);
    if (defined_negative_class) set_negative_class(arg_negative_class);
    if (defined_model_type) set_model_type(arg_model_type);
    if (defined_seed) set_seed(arg_seed);
  }
}



/**
 *  Deallocate memory.
 */
void LinearModel::m__dealloc__() {
  if (lm_) {
    delete lm_;
  }
  delete dt_params_;
  delete py_params_;
}


template <typename T>
void LinearModel::init_dt_model(dt::LType target_ltype /* = dt::LType::MU */) {
  if (lm_) return;

  switch (dt_params_->model_type) {
    case dt::LinearModelType::AUTO :    switch (target_ltype) {
                                          case dt::LType::MU:
                                          case dt::LType::BOOL:    lm_ = new dt::LinearModelBinomial<T>();
                                                                   set_model_type({py::ostring("binomial"), "`LinearModelParams.model_type`"});
                                                                   break;
                                          case dt::LType::INT:
                                          case dt::LType::REAL:    lm_ = new dt::LinearModelRegression<T>();
                                                                   set_model_type({py::ostring("regression"), "`LinearModelParams.model_type`"});
                                                                   break;
                                          case dt::LType::STRING:  lm_ = new dt::LinearModelMultinomial<T>();
                                                                   set_model_type({py::ostring("multinomial"), "`LinearModelParams.model_type`"});
                                                                   break;
                                          default: throw TypeError() << "Target column should have one of "
                                            << "the following ltypes: `void`, `bool`, `int`, `real` or `string`, "
                                            << "instead got: `" << target_ltype << "`";
                                        }
                                        break;

    case dt::LinearModelType::REGRESSION :  lm_ = new dt::LinearModelRegression<T>();
                                            break;

    case dt::LinearModelType::BINOMIAL :    lm_ = new dt::LinearModelBinomial<T>();
                                            break;

    case dt::LinearModelType::MULTINOMIAL : lm_ = new dt::LinearModelMultinomial<T>();
                                            break;
  }
}


/**
 *  .fit(...)
 *  Do dataset validation and a call to `lm_->dispatch_fit(...)` method.
 */

static const char* doc_fit =
R"(fit(self, X_train, y_train, X_validation=None, y_validation=None,
    nepochs_validation=1, validation_error=0.01,
    validation_average_niterations=1)
--

Train model on the input samples and targets using the
`parallel stochastic gradient descent <http://martin.zinkevich.org/publications/nips2010.pdf>`_
method.

Parameters
----------
X_train: Frame
    Training frame.

y_train: Frame
    Target frame having as many rows as `X_train` and one column.

X_validation: Frame
    Validation frame having the same number of columns as `X_train`.

y_validation: Frame
    Validation target frame of shape `(nrows, 1)`.

nepochs_validation: float
    Parameter that specifies how often, in epoch units, validation
    error should be checked.

validation_error: float
    The improvement of the relative validation error that should be
    demonstrated by the model within `nepochs_validation` epochs,
    otherwise the training will stop.

validation_average_niterations: int
    Number of iterations that is used to average the validation error.
    Each iteration corresponds to `nepochs_validation` epochs.

return: LinearModelFitOutput
    `LinearModelFitOutput` is a `Tuple[float, float]` with two fields: `epoch` and `loss`,
    representing the final fitting epoch and the final loss, respectively.
    If validation dataset is not provided, the returned `epoch` equals to
    `nepochs` and the `loss` is just `float('nan')`.

See also
--------
- :meth:`.predict` -- predict for the input samples.
- :meth:`.reset` -- reset the model.

)";

static PKArgs args_fit(2, 5, 0, false, false, {"X_train", "y_train",
                       "X_validation", "y_validation",
                       "nepochs_validation", "validation_error",
                       "validation_average_niterations"}, "fit",
                       doc_fit);

oobj LinearModel::fit(const PKArgs& args) {
  size_t i = 0;
  const Arg& arg_X_train                        = args[i++];
  const Arg& arg_y_train                        = args[i++];
  const Arg& arg_X_validation                   = args[i++];
  const Arg& arg_y_validation                   = args[i++];
  const Arg& arg_nepochs_validation             = args[i++];
  const Arg& arg_validation_error               = args[i++];
  const Arg& arg_validation_average_niterations = args[i++];

  // Training set handling
  if (arg_X_train.is_undefined()) {
    throw ValueError() << "Training frame parameter is missing";
  }

  if (arg_y_train.is_undefined()) {
    throw ValueError() << "Target frame parameter is missing";
  }

  DataTable* dt_X_train = arg_X_train.to_datatable();
  DataTable* dt_y = arg_y_train.to_datatable();

  if (dt_X_train == nullptr || dt_y == nullptr) return py::None();

  if (dt_X_train->ncols() == 0) {
    throw ValueError() << "Training frame must have at least one column";
  }

  if (dt_X_train->nrows() == 0) {
    throw ValueError() << "Training frame cannot be empty";
  }

  if (dt_y->ncols() != 1) {
    throw ValueError() << "Target frame must have exactly one column";
  }

  if (dt_X_train->nrows() != dt_y->nrows()) {
    throw ValueError() << "Target column must have the same number of rows "
                       << "as the training frame";
  }

  if (lm_ && (lm_->get_nfeatures() != dt_X_train->ncols())) {
    throw ValueError() << "This model has already been trained, thus, the "
      << "training frame must have `" << lm_->get_nfeatures() << "` column"
      << ((lm_->get_nfeatures() > 1)? "s" : "")
      << ", instead got: `" << dt_X_train->ncols() << "`";
  }

  dt::LType ltype = dt_y->get_column(0).ltype();
  if (ltype > dt::LType::STRING) {
    throw TypeError() << "Target column should have one of "
      << "the following ltypes: `void`, `bool`, `int`, `real` or `string`, "
      << "instead got: `" << ltype << "`";
  }

  if (dt_params_->model_type == dt::LinearModelType::REGRESSION && ltype > dt::LType::REAL) {
    throw TypeError() << "For regression, target column should have one of "
      << "the following ltypes: `void`, `bool`, `int` or `real`, "
      << "instead got: `" << ltype << "`";
  }


  // Validtion set handling
  DataTable* dt_X_val = nullptr;
  DataTable* dt_y_val = nullptr;
  double nepochs_val = std::numeric_limits<double>::quiet_NaN();
  double val_error = std::numeric_limits<double>::quiet_NaN();
  size_t val_niters = 0;

  if (!arg_X_validation.is_none_or_undefined() &&
      !arg_y_validation.is_none_or_undefined())

  {
    dt::LType ltype_val;
    dt_X_val = arg_X_validation.to_datatable();
    dt_y_val = arg_y_validation.to_datatable();

    if (dt_X_val->ncols() != dt_X_train->ncols()) {
      throw ValueError() << "Validation frame must have the same number of "
                         << "columns as the training frame";
    }


    for (i = 0; i < dt_X_train->ncols(); i++) {
      ltype = dt_X_train->get_column(i).ltype();
      ltype_val = dt_X_val->get_column(i).ltype();

      if (ltype != ltype_val) {
        throw TypeError() << "Training and validation frames must have "
                          << "identical column ltypes, instead for columns `"
                          << dt_X_train->get_names()[i] << "` and `"
                          << dt_X_val->get_names()[i] << "`, got ltypes: `"
                          << ltype << "` and `" << ltype_val << "`";
      }
    }

    if (dt_X_val->nrows() == 0) {
      throw ValueError() << "Validation frame cannot be empty";
    }

    if (dt_y_val->ncols() != 1) {
      throw ValueError() << "Validation target frame must have exactly "
                         << "one column";
    }

    ltype_val = dt_y_val->get_column(0).ltype();

    if (ltype != ltype_val) {
      throw TypeError() << "Training and validation target columns must have "
                        << "the same ltype, got: `" << ltype << "` and `"
                        << ltype_val << "`";
    }

    if (dt_X_val->nrows() != dt_y_val->nrows()) {
      throw ValueError() << "Validation target frame must have the same "
                         << "number of rows as the validation frame itself";
    }

    if (!arg_nepochs_validation.is_none_or_undefined()) {
      nepochs_val = arg_nepochs_validation.to_double();
      py::Validator::check_finite(nepochs_val, arg_nepochs_validation);
      py::Validator::check_positive(nepochs_val, arg_nepochs_validation);
      py::Validator::check_less_than_or_equal_to(
        nepochs_val,
        dt_params_->nepochs,
        arg_nepochs_validation
      );
    } else nepochs_val = 1;

    if (!arg_validation_error.is_none_or_undefined()) {
      val_error = arg_validation_error.to_double();
      py::Validator::check_finite(val_error, arg_validation_error);
      py::Validator::check_positive(val_error, arg_validation_error);
    } else val_error = 0.01;

    if (!arg_validation_average_niterations.is_none_or_undefined()) {
      val_niters = arg_validation_average_niterations.to_size_t();
      py::Validator::check_positive<size_t>(val_niters, arg_validation_average_niterations);
    } else val_niters = 1;
  }

  if (dt_params_->double_precision) {
    init_dt_model<double>(ltype);
  } else {
    init_dt_model<float>(ltype);
  }

  dt::LinearModelFitOutput output = lm_->fit(dt_params_,
                                            dt_X_train, dt_y,
                                            dt_X_val, dt_y_val,
                                            nepochs_val, val_error, val_niters);

  static onamedtupletype py_fit_output_ntt(
    "LinearModelFitOutput",
    "Tuple of fit output",
    {
      {"epoch", "final fitting epoch"},
      {"loss",  "final loss calculated on the validation dataset"}
    }
  );

  py::onamedtuple res(py_fit_output_ntt);
  res.set(0, py::ofloat(output.epoch));
  res.set(1, py::ofloat(output.loss));
  return std::move(res);
}


/**
 *  .predict(...)
 *  Perform dataset validation, make a call to `lm_->predict(...)`,
 *  return frame with predictions.
 */

static const char* doc_predict =
R"(predict(self, X)
--

Predict for the input samples.

Parameters
----------
X: Frame
    A frame to make predictions for. It should have the same number
    of columns as the training frame.

return: Frame
    A new frame of shape `(X.nrows, nlabels)` with the predicted probabilities
    for each row of frame `X` and each of `nlabels` labels
    the model was trained for.

See also
--------
- :meth:`.fit` -- train model on the input samples and targets.
- :meth:`.reset` -- reset the model.

)";

static PKArgs args_predict(1, 0, 0, false, false, {"X"}, "predict",
                           doc_predict);


oobj LinearModel::predict(const PKArgs& args) {
  const Arg& arg_X = args[0];
  if (arg_X.is_undefined()) {
    throw ValueError() << "Frame to make predictions for is missing";
  }

  DataTable* dt_X = arg_X.to_datatable();
  if (dt_X == nullptr) return Py_None;

  if (lm_ == nullptr || !lm_->is_fitted()) {
    throw ValueError() << "Cannot make any predictions, the model "
                          "should be trained first";
  }

  size_t nfeatures = lm_->get_nfeatures();
  if (dt_X->ncols() != nfeatures) {
    throw ValueError() << "Can only predict on a frame that has `" << nfeatures
                       << "` column" << (nfeatures == 1? "" : "s")
                       << ", i.e. the same number of features the model was trained on";
  }

  DataTable* dt_p = lm_->predict(dt_X).release();
  py::oobj df_p = py::Frame::oframe(dt_p);

  return df_p;
}


/**
 *  .reset()
 *  Reset the model by deleting the underlying `dt::LinearModel` object.
 */

static const char* doc_reset =
R"(reset(self)
--

Reset linear model by resetting all the model coefficients and labels.

Parameters
----------
return: None

See also
--------
- :meth:`.fit` -- train model on a dataset.
- :meth:`.predict` -- predict on a dataset.

)";

static PKArgs args_reset(0, 0, 0, false, false, {}, "reset", doc_reset);


void LinearModel::reset(const PKArgs&) {
  if (lm_ != nullptr) {
    delete lm_;
    lm_ = nullptr;
  }
}


/**
 *  .labels
 */
static const char* doc_labels =
R"(
Classification labels the model was trained on.

Parameters
----------
return: Frame
    A one-column frame with the classification labels.
    In the case of numeric regression, the label is
    the target column name.
)";

static GSArgs args_labels(
  "labels",
  doc_labels);


oobj LinearModel::get_labels() const {
  if (lm_ == nullptr || !lm_->is_fitted()) {
    return py::None();
  } else {
    return lm_->get_labels();
  }
}


/**
 *  .is_fitted()
 */

static const char* doc_is_fitted =
R"(is_fitted(self)
--

Report model status.

Parameters
----------
return: bool
    `True` if model is trained, `False` otherwise.

)";

static PKArgs args_is_fitted(0, 0, 0, false, false, {}, "is_fitted", doc_is_fitted);


oobj LinearModel::is_fitted(const PKArgs&) {
  if (lm_ != nullptr && lm_->is_fitted()) {
    return py::True();
  } else {
    return py::False();
  }
}


/**
 *  .model
 */

static const char* doc_model =
R"(
Trained models coefficients.

Parameters
----------
return: Frame
    A frame of shape `(nfeatures + 1, nlabels)`, where `nlabels` is
    the number of labels the model was trained on, and
    `nfeatures` is the number of features. Each column contains
    model coefficients for the corresponding label: starting from
    the intercept and following by the coefficients for each of
    of the `nfeatures` features.
)";

static GSArgs args_model("model", doc_model);

oobj LinearModel::get_model() const {
  if (lm_ == nullptr || !lm_->is_fitted()) {
    return py::None();
  } else {
    return lm_->get_model();
  }
}


void LinearModel::set_model(robj model) {
  DataTable* dt_model = model.to_datatable();
  if (dt_model == nullptr) return;

  size_t is_binomial = dt_params_->model_type == dt::LinearModelType::BINOMIAL;
  if ((dt_model->ncols() + is_binomial) != lm_->get_nlabels()) {
    throw ValueError() << "The number of columns in the model must be consistent "
      << "with the number of labels, instead got: `" << dt_model->ncols()
      << "` and `" << lm_->get_nlabels() << "`, respectively";
  }

  auto stype = dt_params_->double_precision? dt::SType::FLOAT64 : dt::SType::FLOAT32;

  for (size_t i = 0; i < dt_model->ncols(); ++i) {

    const Column& col = dt_model->get_column(i);
    dt::SType c_stype = col.stype();
    if (col.stype() != stype) {
      throw ValueError() << "Column " << i << " in the model frame should "
                         << "have a type of " << stype << ", whereas it has "
                         << "the following type: "
                         << c_stype;
    }

  }
  lm_->set_model(*dt_model);
}



/**
 *  .eta0
 */

static const char* doc_eta0 =
R"(
Learning rate.

Parameters
----------
return: float
    Current `eta0` value.

new_eta0: float
    New `eta0` value, should be positive.

except: ValueError
    The exception is raised when `new_eta0` is not positive.
)";

static GSArgs args_eta0(
  "eta0",
  doc_eta0
);


oobj LinearModel::get_eta0() const {
  return py_params_->get_attr("eta0");
}


void LinearModel::set_eta0(const Arg& py_eta0) {
  double eta0 = py_eta0.to_double();
  py::Validator::check_finite(eta0, py_eta0);
  py::Validator::check_positive(eta0, py_eta0);
  py_params_->replace(0, py_eta0.robj());
  dt_params_->eta0 = eta0;
}


/**
 *  .eta_decay
 */

static const char* doc_eta_decay =
R"(
Decay for the `"time-based"` and `"step-based"` learning rate schedules.

Parameters
----------
return: float
    Current `eta_decay` value.

new_eta_decay: float
    New `eta_decay` value, should be non-negative.

except: ValueError
    The exception is raised when `new_eta_decay` is negative.
)";

static GSArgs args_eta_decay(
  "eta_decay",
  doc_eta_decay
);


oobj LinearModel::get_eta_decay() const {
  return py_params_->get_attr("eta_decay");
}


void LinearModel::set_eta_decay(const Arg& py_eta_decay) {
  double eta_decay = py_eta_decay.to_double();
  py::Validator::check_finite(eta_decay, py_eta_decay);
  py::Validator::check_not_negative(eta_decay, py_eta_decay);
  py_params_->replace(1, py_eta_decay.robj());
  dt_params_->eta_decay = eta_decay;
}


/**
 *  .eta_drop_rate
 */

static const char* doc_eta_drop_rate =
R"(
Drop rate for the `"step-based"` learning rate schedule.

Parameters
----------
return: float
    Current `eta_drop_rate` value.

new_eta_drop_rate: float
    New `eta_drop_rate` value, should be positive.

except: ValueError
    The exception is raised when `new_eta_drop_rate` is not positive.
)";

static GSArgs args_eta_drop_rate(
  "eta_drop_rate",
  doc_eta_drop_rate
);


oobj LinearModel::get_eta_drop_rate() const {
  return py_params_->get_attr("eta_drop_rate");
}


void LinearModel::set_eta_drop_rate(const Arg& py_eta_drop_rate) {
  double eta_drop_rate = py_eta_drop_rate.to_double();
  py::Validator::check_finite(eta_drop_rate, py_eta_drop_rate);
  py::Validator::check_positive(eta_drop_rate, py_eta_drop_rate);
  py_params_->replace(2, py_eta_drop_rate.robj());
  dt_params_->eta_drop_rate = eta_drop_rate;
}


/**
 *  .eta_schedule
 */

static const char* doc_eta_schedule =
R"(
Learning rate `schedule <https://en.wikipedia.org/wiki/Learning_rate#Learning_rate_schedule>`_

- `"constant"` for constant `eta`;
- `"time-based"` for time-based schedule;
- `"step-based"` for step-based schedule;
- `"exponential"` for exponential schedule.

Parameters
----------
return: str
    Current `eta_schedule` value.

new_eta_schedule: "constant" | "time-based" | "step-based" | "exponential"
    New `eta_schedule` value.
)";

static GSArgs args_eta_schedule(
  "eta_schedule",
  doc_eta_schedule
);


oobj LinearModel::get_eta_schedule() const {
  return py_params_->get_attr("eta_schedule");
}


void LinearModel::set_eta_schedule(const Arg& py_eta_schedule) {
  std::string eta_schedule = py_eta_schedule.to_string();
  auto it = py::LearningRateScheduleName.find(eta_schedule);
  if (it == py::LearningRateScheduleName.end()) {
    throw ValueError() << "Learning rate schedule `" << eta_schedule
      << "` is not supported";
  }
  py_params_->replace(3, py_eta_schedule.to_robj());
  dt_params_->eta_schedule = it->second;

}


/**
 *  .lambda1
 */

static const char* doc_lambda1 =
R"(
L1 regularization parameter.

Parameters
----------
return: float
    Current `lambda1` value.

new_lambda1: float
    New `lambda1` value, should be non-negative.

except: ValueError
    The exception is raised when `new_lambda1` is negative.

)";

static GSArgs args_lambda1(
  "lambda1",
  doc_lambda1
);


oobj LinearModel::get_lambda1() const {
  return py_params_->get_attr("lambda1");
}


void LinearModel::set_lambda1(const Arg& py_lambda1) {
  double lambda1 = py_lambda1.to_double();
  py::Validator::check_finite(lambda1, py_lambda1);
  py::Validator::check_not_negative(lambda1, py_lambda1);
  py_params_->replace(4, py_lambda1.to_robj());
  dt_params_->lambda1 = lambda1;
}


/**
 *  .lambda2
 */

static const char* doc_lambda2 =
R"(
L2 regularization parameter.

Parameters
----------
return: float
    Current `lambda2` value.

new_lambda2: float
    New `lambda2` value, should be non-negative.

except: ValueError
    The exception is raised when `new_lambda2` is negative.

)";

static GSArgs args_lambda2(
  "lambda2",
  doc_lambda2
);


oobj LinearModel::get_lambda2() const {
  return py_params_->get_attr("lambda2");
}


void LinearModel::set_lambda2(const Arg& py_lambda2) {
  double lambda2 = py_lambda2.to_double();
  py::Validator::check_finite(lambda2, py_lambda2);
  py::Validator::check_not_negative(lambda2, py_lambda2);
  py_params_->replace(5, py_lambda2.to_robj());
  dt_params_->lambda2 = lambda2;
}



/**
 *  .nepochs
 */

static const char* doc_nepochs =
R"(
Number of training epochs. When `nepochs` is an integer number,
the model will train on all the data provided to :meth:`.fit` method
`nepochs` times. If `nepochs` has a fractional part `{nepochs}`,
the model will train on all the data `[nepochs]` times,
i.e. the integer part of `nepochs`. Plus, it will also perform an additional
training iteration on the `{nepochs}` fraction of data.

Parameters
----------
return: float
    Current `nepochs` value.

new_nepochs: float
    New `nepochs` value, should be non-negative.

except: ValueError
    The exception is raised when `new_nepochs` value is negative.

)";

static GSArgs args_nepochs(
  "nepochs",
  doc_nepochs
);

oobj LinearModel::get_nepochs() const {
  return py_params_->get_attr("nepochs");
}


void LinearModel::set_nepochs(const Arg& arg_nepochs) {
  double nepochs = arg_nepochs.to_double();
  py::Validator::check_finite(nepochs, arg_nepochs);
  py::Validator::check_not_negative(nepochs, arg_nepochs);
  py_params_->replace(6, arg_nepochs.to_robj());
  dt_params_->nepochs = nepochs;
}


/**
 *  .double_precision
 */

static const char* doc_double_precision =
R"(
An option to indicate whether double precision, i.e. `float64`,
or single precision, i.e. `float32`, arithmetic should be
used for computations. This option is read-only and can only be set
during the `LinearModel` object :meth:`construction <datatable.models.LinearModel.__init__>`.

Parameters
----------
return: bool
    Current `double_precision` value.

)";

static GSArgs args_double_precision(
  "double_precision",
  doc_double_precision
);


oobj LinearModel::get_double_precision() const {
  return py_params_->get_attr("double_precision");
}

void LinearModel::set_double_precision(const Arg& arg_double_precision) {
  if (lm_ && lm_->is_fitted()) {
    throw ValueError() << "Cannot change " << arg_double_precision.name()
                       << "for a trained model, "
                       << "reset this model or create a new one";
  }
  bool double_precision = arg_double_precision.to_bool_strict();
  py_params_->replace(7, arg_double_precision.to_robj());
  dt_params_->double_precision = double_precision;
}


/**
 *  .negative_class
 */

static const char* doc_negative_class =
R"(
An option to indicate if a "negative" class should be created
in the case of multinomial classification. For the "negative"
class the model will train on all the negatives, and if
a new label is encountered in the target column, its
coefficients are initialized to the current "negative" class coefficients.
If `negative_class` is set to `False`, the initial coefficients
become zeros.

This option is read-only for a trained model.

Parameters
----------
return: bool
    Current `negative_class` value.

new_negative_class: bool
    New `negative_class` value.

except: ValueError
    The exception is raised when trying to change this option
    for a model that has already been trained.

)";

static GSArgs args_negative_class(
  "negative_class",
  doc_negative_class
);


oobj LinearModel::get_negative_class() const {
  return py_params_->get_attr("negative_class");
}


void LinearModel::set_negative_class(const Arg& arg_negative_class) {
  if (lm_ && lm_->is_fitted()) {
    throw ValueError() << "Cannot change " << arg_negative_class.name()
                       << " for a trained model, "
                       << "reset this model or create a new one";
  }
  bool negative_class = arg_negative_class.to_bool_strict();
  py_params_->replace(8, arg_negative_class.to_robj());
  dt_params_->negative_class = negative_class;
}


/**
 *  .model_type
 */

static const char* doc_model_type =
R"(
A type of the model `LinearModel` should build:

- `"binomial"` for binomial classification;
- `"multinomial"` for multinomial classification;
- `"regression"` for numeric regression;
- `"auto"` for automatic model type detection based on the target column `stype`.

This option is read-only for a trained model.

Parameters
----------
return: str
    Current `model_type` value.

new_model_type: "binomial" | "multinomial" | "regression" | "auto"
    New `model_type` value.

except: ValueError
    The exception is raised when trying to change this option
    for a model that has already been trained.
)";

static GSArgs args_model_type(
  "model_type",
  doc_model_type
);


oobj LinearModel::get_model_type() const {
  return py_params_->get_attr("model_type");
}


void LinearModel::set_model_type(const Arg& py_model_type) {
  if (lm_ && lm_->is_fitted()) {
    throw ValueError() << "Cannot change `model_type` for a trained model, "
                       << "reset this model or create a new one";
  }
  std::string model_type = py_model_type.to_string();
  auto it = py::LinearModelNameType.find(model_type);
  if (it == py::LinearModelNameType.end()) {
    throw ValueError() << "Model type `" << model_type << "` is not supported";
  }

  py_params_->replace(9, py_model_type.to_robj());
  dt_params_->model_type = it->second;
}


/**
 *  .seed
 */

static const char* doc_seed =
R"(
Seed for the quasi-random number generator that is used for
data shuffling when fitting the model. If seed is `0`,
no shuffling is performed.

Parameters
----------
return: int
    Current `seed` value.

new_seed: int
    New `seed` value, should be non-negative.

)";

static GSArgs args_seed(
  "seed",
  doc_seed
);


oobj LinearModel::get_seed() const {
  return py_params_->get_attr("seed");
}


void LinearModel::set_seed(const Arg& arg_seed) {
  size_t seed = arg_seed.to_size_t();
  py_params_->replace(10, arg_seed.to_robj());
  dt_params_->seed = static_cast<unsigned int>(seed);
}


/**
 *  .params
 */

static const char* doc_params =
R"(
`LinearModel` model parameters as a named tuple `LinearModelParams`,
see :meth:`.__init__` for more details.
This option is read-only for a trained model.

Parameters
----------
return: LinearModelParams
    Current `params` value.

new_params: LinearModelParams
    New `params` value.

except: ValueError
    The exception is raised when

    - trying to change this option for a model that has already been trained;
    - individual parameter values are incompatible with the corresponding setters.

)";

static GSArgs args_params(
  "params",
  doc_params
);


oobj LinearModel::get_params_namedtuple() const {
  return *py_params_;
}


void LinearModel::set_params_namedtuple(robj params_in) {
  py::otuple params_tuple = params_in.to_otuple();
  size_t n_params = params_tuple.size();

  if (n_params != N_PARAMS) {
    throw ValueError() << "Tuple of LinearModel parameters should have "
      << "`" << N_PARAMS << "` elements, instead got: " << n_params;
  }
  py::oobj py_eta0 = params_in.get_attr("eta0");
  py::oobj py_eta_decay = params_in.get_attr("eta_decay");
  py::oobj py_eta_drop_rate = params_in.get_attr("eta_drop_rate");
  py::oobj py_eta_schedule = params_in.get_attr("eta_schedule");
  py::oobj py_lambda1 = params_in.get_attr("lambda1");
  py::oobj py_lambda2 = params_in.get_attr("lambda2");
  py::oobj py_nepochs = params_in.get_attr("nepochs");
  py::oobj py_double_precision = params_in.get_attr("double_precision");
  py::oobj py_negative_class = params_in.get_attr("negative_class");
  py::oobj py_model_type = params_in.get_attr("model_type");
  py::oobj py_seed = params_in.get_attr("seed");

  set_eta0({py_eta0, "`LinearModelParams.eta0`"});
  set_eta_decay({py_eta_decay, "`LinearModelParams.eta_decay`"});
  set_eta_drop_rate({py_eta_drop_rate, "`LinearModelParams.eta_drop_rate`"});
  set_eta_schedule({py_eta_schedule, "`LinearModelParams.eta_schedule`"});
  set_lambda1({py_lambda1, "`LinearModelParams.lambda1`"});
  set_lambda2({py_lambda2, "`LinearModelParams.lambda2`"});
  set_nepochs({py_nepochs, "`LinearModelParams.nepochs`"});
  set_double_precision({py_double_precision, "`LinearModelParams.double_precision`"});
  set_negative_class({py_negative_class, "`LinearModelParams.negative_class`"});
  set_model_type({py_model_type, "`LinearModelParams.model_type`"});
  set_seed({py_seed, "`LinearModelParams.seed`"});
}


oobj LinearModel::get_params_tuple() const {
  return otuple {get_eta0(),
                 get_eta_decay(),
                 get_eta_drop_rate(),
                 get_eta_schedule(),
                 get_lambda1(),
                 get_lambda2(),
                 get_nepochs(),
                 get_double_precision(),
                 get_negative_class(),
                 get_model_type(),
                 get_seed()
                };
}


void LinearModel::set_params_tuple(robj params) {
  size_t i = 0;
  py::otuple params_tuple = params.to_otuple();
  size_t n_params = params_tuple.size();
  if (n_params != N_PARAMS) {
    throw ValueError() << "Tuple of `LinearModel` parameters should have "
      << "` " << N_PARAMS << "` elements, instead got: " << n_params;
  }
  set_eta0({params_tuple[i++], "eta0"});
  set_eta_decay({params_tuple[i++], "eta_decay"});
  set_eta_drop_rate({params_tuple[i++], "eta_drop_rate"});
  set_eta_schedule({params_tuple[i++], "eta_schedule"});
  set_lambda1({params_tuple[i++], "lambda1"});
  set_lambda2({params_tuple[i++], "lambda2"});
  set_nepochs({params_tuple[i++], "nepochs"});
  set_double_precision({params_tuple[i++], "double_precision"});
  set_negative_class({params_tuple[i++], "negative_class"});
  set_model_type({params_tuple[i++], "model_type"});
  set_seed({params_tuple[i++], "seed"});
  xassert(i == N_PARAMS);
}


void LinearModel::init_params() {
  static onamedtupletype py_params_ntt(
    "LinearModelParams",
    args_params.doc, {
      {args_eta0.name,              args_eta0.doc},
      {args_eta_decay.name,        args_eta_decay.doc},
      {args_eta_drop_rate.name,    args_eta_drop_rate.doc},
      {args_eta_schedule.name,     args_eta_schedule.doc},
      {args_lambda1.name,          args_lambda1.doc},
      {args_lambda2.name,          args_lambda2.doc},
      {args_nepochs.name,          args_nepochs.doc},
      {args_double_precision.name, args_double_precision.doc},
      {args_negative_class.name,   args_negative_class.doc},
      {args_model_type.name,       args_model_type.doc},
      {args_seed.name,             args_seed.doc}
    }
  );

  dt_params_ = new dt::LinearModelParams();
  py_params_ = new py::onamedtuple(py_params_ntt);
  size_t i = 0;

  py_params_->replace(i++, py::ofloat(dt_params_->eta0));
  py_params_->replace(i++, py::ofloat(dt_params_->eta_decay));
  py_params_->replace(i++, py::ofloat(dt_params_->eta_drop_rate));
  py_params_->replace(i++, py::ostring("constant"));
  py_params_->replace(i++, py::ofloat(dt_params_->lambda1));
  py_params_->replace(i++, py::ofloat(dt_params_->lambda2));
  py_params_->replace(i++, py::ofloat(dt_params_->nepochs));
  py_params_->replace(i++, py::obool(dt_params_->double_precision));
  py_params_->replace(i++, py::obool(dt_params_->negative_class));
  py_params_->replace(i++, py::ostring("auto"));
  py_params_->replace(i++, py::oint(static_cast<size_t>(dt_params_->seed)));
  xassert(i == N_PARAMS);
}



/**
 *  Pickling support.
 */
static PKArgs args___getstate__(
    0, 0, 0, false, false, {}, "__getstate__", nullptr);


oobj LinearModel::m__getstate__(const PKArgs&) {
  py::oobj py_api_version = py::oint(API_VERSION);
  py::oobj py_labels = get_labels();
  py::oobj py_params_tuple = get_params_tuple();
  py::oobj py_model = get_model();

  return otuple {py_api_version,
                 py_params_tuple,
                 py_labels,
                 py_model
                };
}


/**
 *  Unpickling support.
 */
static PKArgs args___setstate__(
    1, 0, 0, false, false, {"state"}, "__setstate__", nullptr);

void LinearModel::m__setstate__(const PKArgs& args) {
  py::otuple pickle = args[0].to_otuple();
  py::oint py_api_version = pickle[0].to_size_t(); // Not used for the moment
  init_params();
  set_params_tuple(pickle[1]);

  // Set up labels and model coefficients if available
  if (pickle[2].is_frame()) {
    xassert(dt_params_->model_type > dt::LinearModelType::AUTO);

    if (dt_params_->double_precision) {
      init_dt_model<double>();
    } else {
      init_dt_model<float>();
    }

    lm_->set_labels(*pickle[2].to_datatable());
    set_model(pickle[3]);
  }
}



//------------------------------------------------------------------------------
// py::LinearModel::Type
//------------------------------------------------------------------------------

void LinearModel::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.models.LinearModel");
  xt.set_class_doc(dt::doc_models_LM);

  xt.add(CONSTRUCTOR(&LinearModel::m__init__, args___init__));
  xt.add(DESTRUCTOR(&LinearModel::m__dealloc__));

  // Input parameters
  xt.add(GETTER(&LinearModel::get_params_namedtuple, args_params));
  xt.add(GETSET(&LinearModel::get_eta0, &LinearModel::set_eta0, args_eta0));
  xt.add(GETSET(&LinearModel::get_eta_decay, &LinearModel::set_eta_decay, args_eta_decay));
  xt.add(GETSET(&LinearModel::get_eta_drop_rate, &LinearModel::set_eta_drop_rate, args_eta_drop_rate));
  xt.add(GETSET(&LinearModel::get_eta_schedule, &LinearModel::set_eta_schedule, args_eta_schedule));
  xt.add(GETSET(&LinearModel::get_lambda1, &LinearModel::set_lambda1, args_lambda1));
  xt.add(GETSET(&LinearModel::get_lambda2, &LinearModel::set_lambda2, args_lambda2));
  xt.add(GETSET(&LinearModel::get_nepochs, &LinearModel::set_nepochs, args_nepochs));
  xt.add(GETTER(&LinearModel::get_double_precision, args_double_precision));
  xt.add(GETSET(&LinearModel::get_negative_class, &LinearModel::set_negative_class, args_negative_class));
  xt.add(GETSET(&LinearModel::get_seed, &LinearModel::set_seed, args_seed));
  xt.add(GETSET(&LinearModel::get_model_type, &LinearModel::set_model_type, args_model_type));

  // Model and labels
  xt.add(GETTER(&LinearModel::get_labels, args_labels));
  xt.add(GETTER(&LinearModel::get_model, args_model));

  // Fit, predict and reset
  xt.add(METHOD(&LinearModel::fit, args_fit));
  xt.add(METHOD(&LinearModel::predict, args_predict));
  xt.add(METHOD(&LinearModel::reset, args_reset));
  xt.add(METHOD(&LinearModel::is_fitted, args_is_fitted));

  // Pickling and unpickling
  xt.add(METHOD(&LinearModel::m__getstate__, args___getstate__));
  xt.add(METHOD(&LinearModel::m__setstate__, args___setstate__));
}




} // namespace py
