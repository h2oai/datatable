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

static PKArgs args___init__(0, 1, LinearModel::N_PARAMS, false, false,
                                 {"params",
                                 "eta0", "eta_decay", "eta_drop_rate", "eta_schedule",
                                 "lambda1", "lambda2",
                                 "nepochs", "double_precision", "negative_class",
                                 "model_type",
                                 "seed"},
                                 "__init__", dt::doc_models_LinearModel___init__);


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
    case dt::LinearModelType::AUTO :
      switch (target_ltype) {
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

static PKArgs args_fit(2, 5, 0, false, false, {"X_train", "y_train",
                       "X_validation", "y_validation",
                       "nepochs_validation", "validation_error",
                       "validation_average_niterations"}, "fit",
                       dt::doc_models_LinearModel_fit);

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

static PKArgs args_predict(1, 0, 0, false, false, {"X"}, "predict",
                           dt::doc_models_LinearModel_predict);


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

static PKArgs args_reset(0, 0, 0, false, false, {}, "reset",
                         dt::doc_models_LinearModel_reset);


void LinearModel::reset(const PKArgs&) {
  if (lm_ != nullptr) {
    delete lm_;
    lm_ = nullptr;
  }
}


/**
 *  .labels
 */

static GSArgs args_labels(
  "labels",
  dt::doc_models_LinearModel_labels);


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

static PKArgs args_is_fitted(0, 0, 0, false, false, {}, "is_fitted",
                             dt::doc_models_LinearModel_is_fitted);


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

static GSArgs args_model("model", dt::doc_models_LinearModel_model);

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

static GSArgs args_eta0(
  "eta0",
  dt::doc_models_LinearModel_eta0
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

static GSArgs args_eta_decay(
  "eta_decay",
  dt::doc_models_LinearModel_eta_decay
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

static GSArgs args_eta_drop_rate(
  "eta_drop_rate",
  dt::doc_models_LinearModel_eta_drop_rate
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

static GSArgs args_eta_schedule(
  "eta_schedule",
  dt::doc_models_LinearModel_eta_schedule
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

static GSArgs args_lambda1(
  "lambda1",
  dt::doc_models_LinearModel_lambda1
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

static GSArgs args_lambda2(
  "lambda2",
  dt::doc_models_LinearModel_lambda2
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

static GSArgs args_nepochs(
  "nepochs",
  dt::doc_models_LinearModel_nepochs
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

static GSArgs args_double_precision(
  "double_precision",
  dt::doc_models_LinearModel_double_precision
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

static GSArgs args_negative_class(
  "negative_class",
  dt::doc_models_LinearModel_negative_class
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

static GSArgs args_model_type(
  "model_type",
  dt::doc_models_LinearModel_model_type
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

static GSArgs args_seed(
  "seed",
  dt::doc_models_LinearModel_seed
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

static GSArgs args_params("params", dt::doc_models_LinearModel_params);

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
  xt.set_class_doc(dt::doc_models_LinearModel);

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
