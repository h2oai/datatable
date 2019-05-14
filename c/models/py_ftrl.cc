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
#include <vector>
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/string.h"
#include "str/py_str.h"
#include "models/py_ftrl.h"
#include "models/py_validator.h"
#include "models/utils.h"

namespace py {

PKArgs Ftrl::Type::args___init__(0, 1, 10, false, false,
                                 {"params", "alpha", "beta", "lambda1",
                                 "lambda2", "nbins", "mantissa_nbits",
                                 "nepochs", "double_precision",
                                 "negative_class", "interactions"},
                                 "__init__", nullptr);


/**
 *  Ftrl(...)
 *  Initialize Ftrl object with the provided parameters.
 */
void Ftrl::m__init__(PKArgs& args) {
  dtft = nullptr;
  double_precision = dt::FtrlParams().double_precision;

  const Arg& arg_params           = args[0];
  const Arg& arg_alpha            = args[1];
  const Arg& arg_beta             = args[2];
  const Arg& arg_lambda1          = args[3];
  const Arg& arg_lambda2          = args[4];
  const Arg& arg_nbins            = args[5];
  const Arg& arg_mantissa_nbits   = args[6];
  const Arg& arg_nepochs          = args[7];
  const Arg& arg_double_precision = args[8];
  const Arg& arg_negative_class   = args[9];
  const Arg& arg_interactions     = args[10];

  bool defined_params           = !arg_params.is_none_or_undefined();
  bool defined_alpha            = !arg_alpha.is_none_or_undefined();
  bool defined_beta             = !arg_beta.is_none_or_undefined();
  bool defined_lambda1          = !arg_lambda1.is_none_or_undefined();
  bool defined_lambda2          = !arg_lambda2.is_none_or_undefined();
  bool defined_nbins            = !arg_nbins.is_none_or_undefined();
  bool defined_mantissa_nbits   = !arg_mantissa_nbits.is_none_or_undefined();
  bool defined_nepochs          = !arg_nepochs.is_none_or_undefined();
  bool defined_double_precision = !arg_double_precision.is_none_or_undefined();
  bool defined_negative_class   = !arg_negative_class.is_none_or_undefined();
  bool defined_interactions     = !arg_interactions.is_none_or_undefined();
  bool defined_individual_param = defined_alpha || defined_beta ||
                                  defined_lambda1 || defined_lambda2 ||
                                  defined_nbins || defined_mantissa_nbits ||
                                  defined_nepochs || defined_double_precision ||
                                  defined_negative_class || defined_interactions;

  init_py_params();

  if (defined_params) {
    if (defined_individual_param) {
      throw TypeError() << "You can either pass all the parameters with "
        << "`params` or any of the individual parameters with `alpha`, "
        << "`beta`, `lambda1`, `lambda2`, `nbins`, `mantissa_nbits`, `nepochs`, "
        << "`double_precision`, `negative_class` or `interactions` to Ftrl constructor, "
        << "but not both at the same time";
    }

    py::otuple py_params_in = arg_params.to_otuple();
    py::oobj py_double_precision = py_params_in.get_attr("double_precision");
    double_precision = py_double_precision.to_bool_strict();

    init_dt_ftrl();
    set_params_namedtuple(py_params_in);

  } else {
    if (defined_double_precision) {
      double_precision = arg_double_precision.to_bool_strict();
    }

    init_dt_ftrl();
    if (defined_alpha) set_alpha(arg_alpha);
    if (defined_beta) set_beta(arg_beta);
    if (defined_lambda1) set_lambda1(arg_lambda1);
    if (defined_lambda2) set_lambda2(arg_lambda2);
    if (defined_nbins) set_nbins(arg_nbins);
    if (defined_mantissa_nbits) set_mantissa_nbits(arg_mantissa_nbits);
    if (defined_nepochs) set_nepochs(arg_nepochs);
    if (defined_double_precision) set_double_precision(arg_double_precision);
    if (defined_negative_class) set_negative_class(arg_negative_class);
    if (defined_interactions) set_interactions(arg_interactions);
  }
}


void Ftrl::init_dt_ftrl() {
  if (double_precision) {
    dtft = new dt::Ftrl<double>();
  } else {
    dtft = new dt::Ftrl<float>();
  }
}


void Ftrl::init_py_params() {
  dt::FtrlParams params;
  py::onamedtuple py_params_temp(py_ntt);
  py_params = std::move(py_params_temp);

  py_params.replace(0, py::ofloat(params.alpha));
  py_params.replace(1, py::ofloat(params.beta));
  py_params.replace(2, py::ofloat(params.lambda1));
  py_params.replace(3, py::ofloat(params.lambda2));
  py_params.replace(4, py::oint(static_cast<size_t>(params.nbins)));
  py_params.replace(5, py::oint(params.mantissa_nbits));
  py_params.replace(6, py::oint(params.nepochs));
  py_params.replace(7, py::obool(params.double_precision));
  py_params.replace(8, py::obool(params.negative_class));
  py_params.replace(9, py::None());
}


/**
 *  Deallocate underlying data for an Ftrl object
 */
void Ftrl::m__dealloc__() {
  delete dtft;
  dtft = nullptr;
}


/**
 *  Check if provided interactions are consistent with the column names
 *  of the training frame.
 */
void Ftrl::init_dt_interactions() {
  std::vector<sizetvec> interactions;
  auto py_iter = py_params.get_attr("interactions").to_oiter();
  interactions.reserve(py_iter.size());

  for (auto py_interaction : py_iter) {
    size_t nfeatures = py_interaction.to_pylist().size();
    sizetvec interaction;
    interaction.reserve(nfeatures);
    for (auto py_feature : py_interaction.to_oiter()) {
      std::string feature = py_feature.to_string();

      auto it = find(colnames.begin(), colnames.end(), feature);
      if (it == colnames.end()) {
        throw ValueError() << "Feature " << py_feature << " is used for "
                              "interactions, however, it is missing in the "
                              "training frame";
      }

      auto pos = static_cast<size_t>(std::distance(colnames.begin(), it));
      interaction.push_back(pos);
    }

    interactions.push_back(std::move(interaction));
  }
  dtft->set_interactions(std::move(interactions));
}


/**
 *  .fit(...)
 *  Do dataset validation and a call to `dtft->dispatch_fit(...)` method.
 */
static PKArgs args_fit(2, 5, 0, false, false, {"X_train", "y_train",
                       "X_validation", "y_validation",
                       "nepochs_validation", "validation_error",
                       "validation_average_niterations"}, "fit",
R"(fit(self, X_train, y_train, X_validation=None, y_validation=None, nepochs_validation=1, validation_error=0.01, validation_average_niterations=1)
--

Train FTRL model on a dataset.

Parameters
----------
X_train: Frame
    Training frame of shape (nrows, ncols).

y_train: Frame
    Target frame of shape (nrows, 1).

X_validation: Frame
    Validation frame of shape (nrows, ncols).

y_validation: Frame
    Validation target frame of shape (nrows, 1).

nepochs_validation: float
    Parameter that specifies how often, in epoch units, validation
    error should be checked.

validation_error: float
    If within `nepochs_validation` relative validation error does not improve
    by at least `validation_error`, training stops.

validation_average_niterations: int
    Number of iterations that is used to calculate average loss. Here, each
    iteration corresponds to `nepochs_validation` epochs.

Returns
-------
A tuple consisting of two elements: `epoch` and `loss`, where
`epoch` is the epoch at which model fitting stopped, and `loss` is the final
loss. When validation dataset is not provided, `epoch` returned is equal to
`nepochs`, and `loss` is `float('nan')`.
)");


oobj Ftrl::fit(const PKArgs& args) {
  const Arg& arg_X_train                        = args[0];
  const Arg& arg_y_train                        = args[1];
  const Arg& arg_X_validation                   = args[2];
  const Arg& arg_y_validation                   = args[3];
  const Arg& arg_nepochs_validation             = args[4];
  const Arg& arg_validation_error               = args[5];
  const Arg& arg_validation_average_niterations = args[6];

  // Training set handling
  if (arg_X_train.is_undefined()) {
    throw ValueError() << "Training frame parameter is missing";
  }

  if (arg_y_train.is_undefined()) {
    throw ValueError() << "Target frame parameter is missing";
  }

  DataTable* dt_X = arg_X_train.to_datatable();
  DataTable* dt_y = arg_y_train.to_datatable();

  if (dt_X == nullptr || dt_y == nullptr) return py::None();

  if (dt_X->ncols == 0) {
    throw ValueError() << "Training frame must have at least one column";
  }

  if (dt_X->nrows == 0) {
    throw ValueError() << "Training frame cannot be empty";
  }

  if (dt_y->ncols != 1) {
    throw ValueError() << "Target frame must have exactly one column";
  }

  if (dt_X->nrows != dt_y->nrows) {
    throw ValueError() << "Target column must have the same number of rows "
                       << "as the training frame";
  }

  if (!dtft->is_trained()) {
    colnames = dt_X->get_names();
  }

  if (dtft->is_trained() && dt_X->get_names() != colnames) {
    throw ValueError() << "Training frame names cannot change for a trained "
                       << "model";
  }

  if (!py_params.get_attr("interactions").is_none()
      && !dtft->get_interactions().size()) {
    init_dt_interactions();
  }

  // Validtion set handling
  DataTable* dt_X_val = nullptr;
  DataTable* dt_y_val = nullptr;
  double nepochs_val = std::numeric_limits<double>::quiet_NaN();
  double val_error = std::numeric_limits<double>::quiet_NaN();
  size_t val_niters = 0;

  if (!arg_X_validation.is_none_or_undefined() &&
      !arg_y_validation.is_none_or_undefined()) {
    dt_X_val = arg_X_validation.to_datatable();
    dt_y_val = arg_y_validation.to_datatable();

    if (dt_X_val->ncols != dt_X->ncols) {
      throw ValueError() << "Validation frame must have the same number of "
                         << "columns as the training frame";
    }

    if (dt_X_val->get_names() != colnames) {
      throw ValueError() << "Validation frame must have the same column "
                         << "names as the training frame";
    }

    if (dt_X_val->nrows == 0) {
      throw ValueError() << "Validation frame cannot be empty";
    }

    if (dt_y_val->ncols != 1) {
      throw ValueError() << "Validation target frame must have exactly "
                         << "one column";
    }

    if (dt_y_val->columns[0]->stype() != dt_y->columns[0]->stype()) {
      throw ValueError() << "Validation target frame must have the same "
                            "stype as the target frame";
    }

    if (dt_X_val->nrows != dt_y_val->nrows) {
      throw ValueError() << "Validation target frame must have the same "
                         << "number of rows as the validation frame itself";
    }

    if (!arg_nepochs_validation.is_none_or_undefined()) {
      nepochs_val = arg_nepochs_validation.to_double();
      py::Validator::check_positive<double>(nepochs_val, arg_nepochs_validation);
      py::Validator::check_less_than_or_equal_to<double>(
        nepochs_val,
        dtft->get_nepochs(),
        arg_nepochs_validation
      );
    } else nepochs_val = 1;

    if (!arg_validation_error.is_none_or_undefined()) {
      val_error = arg_validation_error.to_double();
      // py::Validator::check_positive<double>(val_error, arg_validation_error);
    } else val_error = 0.01;

    if (!arg_validation_average_niterations.is_none_or_undefined()) {
      val_niters = arg_validation_average_niterations.to_size_t();
      py::Validator::check_positive<size_t>(val_niters, arg_validation_average_niterations);
    } else val_niters = 1;
  }

  dt::FtrlFitOutput output = dtft->dispatch_fit(dt_X, dt_y,
                                                dt_X_val, dt_y_val,
                                                nepochs_val, val_error, val_niters);

  static onamedtupletype ntt(
    "FtrlFitOutput",
    "Tuple of fit output",
    {{"epoch", "epoch at which fitting stopped"},
     {"loss",  "final loss calculated on the validation dataset"}}
  );

  py::onamedtuple res(ntt);
  res.set(0, py::ofloat(output.epoch));
  res.set(1, py::ofloat(output.loss));
  return std::move(res);
}


/**
 *  .predict(...)
 *  Perform dataset validation, make a call to `dtft->predict(...)`,
 *  return frame with predictions.
 */
static PKArgs args_predict(1, 0, 0, false, false, {"X"}, "predict",
R"(predict(self, X)
--

Make predictions for a dataset.

Parameters
----------
X: Frame
    Frame of shape (nrows, ncols) to make predictions for.
    It should have the same number of columns as the training frame.

Returns
-------
A new frame of shape (nrows, nlabels) with the predicted probabilities
for each row of frame X and each label the model was trained for.
)");


oobj Ftrl::predict(const PKArgs& args) {
  const Arg& arg_X = args[0];
  if (arg_X.is_undefined()) {
    throw ValueError() << "Frame to make predictions for is missing";
  }

  DataTable* dt_X = arg_X.to_datatable();
  if (dt_X == nullptr) return Py_None;

  if (!dtft->is_trained()) {
    throw ValueError() << "Cannot make any predictions, the model "
                          "should be trained first";
  }

  size_t ncols = dtft->get_ncols();
  if (dt_X->ncols != ncols && ncols != 0) {
    throw ValueError() << "Can only predict on a frame that has " << ncols
                       << " column" << (ncols == 1? "" : "s")
                       << ", i.e. has the same number of features as "
                          "was used for model training";
  }

  if (dt_X->get_names() != colnames) {
    throw ValueError() << "Frames used for training and predictions "
                       << "should have the same column names";
  }

  if (!py_params.get_attr("interactions").is_none()
      && !dtft->get_interactions().size()) {
    init_dt_interactions();
  }


  if (!py_params.get_attr("interactions").is_none()
      && !dtft->get_interactions().size()) {
    init_dt_interactions();
  }


  DataTable* dt_p = dtft->predict(dt_X).release();
  py::oobj df_p = py::oobj::from_new_reference(
                         py::Frame::from_datatable(dt_p)
                  );

  return df_p;
}


/**
 *  .reset()
 *  Reset the model by making a call to `dtft->reset()`.
 */
static PKArgs args_reset(0, 0, 0, false, false, {}, "reset",
R"(reset(self)
--

Reset FTRL model by clearing all the model weights, labels and
feature importance information.

Parameters
----------
None

Returns
-------
None
)");


void Ftrl::reset(const PKArgs&) {
  dtft->reset();
  colnames.clear();
}


/**
 *  .labels
 */
static GSArgs args_labels(
  "labels",
  R"(List of labels used for classification.)");


oobj Ftrl::get_labels() const {
  if (dtft->is_trained()) {
    const strvec& labels = dtft->get_labels();
    size_t nlabels = labels.size();

    py::olist py_labels(nlabels);
    for (size_t i = 0; i < nlabels; ++i) {
      py::ostring py_label = py::ostring(labels[i]);
      py_labels.set(i, std::move(py_label));
    }
    return std::move(py_labels);
  } else {
    return py::None();
  }
}


void Ftrl::set_labels(robj py_labels) {
  if (py_labels.is_list()) {
    py::olist py_labels_list = py_labels.to_pylist();
    size_t nlabels = py_labels_list.size();

    strvec labels(nlabels);
    for (size_t i = 0; i < nlabels; ++i) {
      labels[i] = py_labels_list[i].to_string();
    }
    dtft->set_labels(labels);
  }
}



/**
 *  .model
 */
static GSArgs args_model(
  "model",
R"(Model frame of shape `(nbins, 2 * nlabels)`, where nlabels is
the total number of labels the model was trained on, and nbins
is the number of bins used for the hashing trick. Odd frame columns
contain z model coefficients, and even columns n model coefficients.)");


oobj Ftrl::get_model() const {
  if (!dtft->is_trained()) return py::None();

  DataTable* dt_model = dtft->get_model();
  py::oobj df_model = py::oobj::from_new_reference(
                        py::Frame::from_datatable(dt_model)
                      );
  return df_model;
}


void Ftrl::set_model(robj model) {
  DataTable* dt_model = model.to_datatable();
  if (dt_model == nullptr) return;

  size_t ncols = dt_model->ncols;
  if (dt_model->nrows != dtft->get_nbins() || dt_model->ncols%2 != 0) {
    throw ValueError() << "Model frame must have " << dtft->get_nbins()
                       << " rows, and an even number of columns, "
                       << "whereas your frame has "
                       << dt_model->nrows << " row"
                       << (dt_model->nrows == 1? "": "s")
                       << " and "
                       << dt_model->ncols << " column"
                       << (dt_model->ncols == 1? "": "s");

  }

  SType stype = (double_precision)? SType::FLOAT64 : SType::FLOAT32;
  bool (*has_negatives)(const Column*) = (double_precision)?
                                         py::Validator::has_negatives<double>:
                                         py::Validator::has_negatives<float>;

  for (size_t i = 0; i < ncols; ++i) {
    Column* col = dt_model->columns[i];
    SType c_stype = col->stype();
    if (col->stype() != stype) {
      throw ValueError() << "Column " << i << " in the model frame should "
                         << "have a type of " << stype << ", whereas it has "
                         << "the following type: "
                         << c_stype;
    }

    if ((i % 2) && has_negatives(col)) {
      throw ValueError() << "Column " << i << " cannot have negative values";
    }
  }
  dtft->set_model(dt_model);
}


/**
 *  .feature_importances
 */
static GSArgs args_fi(
  "feature_importances",
R"(Two-column frame with feature names and the corresponding
feature importances normalized to [0; 1].)");


oobj Ftrl::get_fi() const {
  return get_normalized_fi(true);
}

oobj Ftrl::get_normalized_fi(bool normalize) const {
  if (!dtft->is_trained()) return py::None();

  DataTable* dt_fi = dtft->get_fi(normalize);
  py::oobj df_fi = py::oobj::from_new_reference(
                     py::Frame::from_datatable(dt_fi)
                   );
  return df_fi;
}


/**
 *  .colnames
 */
static GSArgs args_colnames(
  "colnames",
  "Column names"
);


oobj Ftrl::get_colnames() const {
  if (dtft->is_trained()) {
    size_t ncols = colnames.size();
    py::olist py_colnames(ncols);
    for (size_t i = 0; i < ncols; ++i) {
      py_colnames.set(i, py::ostring(colnames[i]));
    }
    return std::move(py_colnames);
  } else {
    return py::None();
  }
}


void Ftrl::set_colnames(robj py_colnames) {
  if (py_colnames.is_list()) {
    py::olist py_colnames_list = py_colnames.to_pylist();
    size_t ncolnames = py_colnames_list.size();

    colnames.reserve(ncolnames);
    for (size_t i = 0; i < ncolnames; ++i) {
      colnames.push_back(py_colnames_list[i].to_string());
    }
  }
}


/**
 *  .colname_hashes
 */
static GSArgs args_colname_hashes(
  "colname_hashes",
  "Column name hashes"
);


oobj Ftrl::get_colname_hashes() const {
  if (dtft->is_trained()) {
    size_t ncols = dtft->get_ncols();
    py::olist py_colname_hashes(ncols);
    const std::vector<uint64_t>& colname_hashes = dtft->get_colname_hashes();
    for (size_t i = 0; i < ncols; ++i) {
      size_t h = static_cast<size_t>(colname_hashes[i]);
      py_colname_hashes.set(i, py::oint(h));
    }
    return std::move(py_colname_hashes);
  } else {
    return py::None();
  }
}


/**
 *  .alpha
 */
static GSArgs args_alpha(
  "alpha",
  "`alpha` in per-coordinate FTRL-Proximal algorithm");


oobj Ftrl::get_alpha() const {
  return py_params.get_attr("alpha");
}


void Ftrl::set_alpha(const Arg& py_alpha) {
  double alpha = py_alpha.to_double();
  py::Validator::check_positive(alpha, py_alpha);
  dtft->set_alpha(alpha);
  py_params.replace(0, py_alpha.robj());
}


/**
 *  .beta
 */
static GSArgs args_beta(
  "beta",
  "`beta` in per-coordinate FTRL-Proximal algorithm");


oobj Ftrl::get_beta() const {
  return py_params.get_attr("beta");
}


void Ftrl::set_beta(const Arg& py_beta) {
  double beta = py_beta.to_double();
  py::Validator::check_not_negative(beta, py_beta);
  dtft->set_beta(beta);
  py_params.replace(1, py_beta.to_robj());
}


/**
 *  .lambda1
 */
static GSArgs args_lambda1(
  "lambda1",
  "L1 regularization parameter");


oobj Ftrl::get_lambda1() const {
  return py_params.get_attr("lambda1");
}


void Ftrl::set_lambda1(const Arg& py_lambda1) {
  double lambda1 = py_lambda1.to_double();
  py::Validator::check_not_negative(lambda1, py_lambda1);
  dtft->set_lambda1(lambda1);
  py_params.replace(2, py_lambda1.to_robj());
}


/**
 *  .lambda2
 */
static GSArgs args_lambda2(
  "lambda2",
  "L2 regularization parameter");


oobj Ftrl::get_lambda2() const {
  return py_params.get_attr("lambda2");
}


void Ftrl::set_lambda2(const Arg& py_lambda2) {
  double lambda2 = py_lambda2.to_double();
  py::Validator::check_not_negative(lambda2, py_lambda2);
  dtft->set_lambda2(lambda2);
  py_params.replace(3, py_lambda2.to_robj());
}


/**
 *  .nbins
 */
static GSArgs args_nbins(
  "nbins",
  "Number of bins for the hashing trick");


oobj Ftrl::get_nbins() const {
  return py_params.get_attr("nbins");
}


void Ftrl::set_nbins(const Arg& py_nbins) {
  if (dtft->is_trained()) {
    throw ValueError() << "Cannot change `nbins` for a trained model, "
                       << "reset this model or create a new one";
  }

  size_t nbins = py_nbins.to_size_t();
  py::Validator::check_positive(nbins, py_nbins);
  dtft->set_nbins(static_cast<uint64_t>(nbins));
  py_params.replace(4, py_nbins.to_robj());
}


/**
 *  .mantissa_nbits
 */
static GSArgs args_mantissa_nbits(
  "mantissa_nbits",
  "Number of bits from mantissa to be used for hashing float values");


oobj Ftrl::get_mantissa_nbits() const {
  return py_params.get_attr("mantissa_nbits");
}


void Ftrl::set_mantissa_nbits(const Arg& py_mantissa_nbits) {
  if (dtft->is_trained()) {
    throw ValueError() << "Cannot change `mantissa_nbits` for a trained model, "
                       << "reset this model or create a new one";
  }

  size_t mantissa_nbits = py_mantissa_nbits.to_size_t();
  py::Validator::check_less_than_or_equal_to<uint64_t>(
    mantissa_nbits,
    dt::FtrlBase::DOUBLE_MANTISSA_NBITS,
    py_mantissa_nbits
  );
  dtft->set_mantissa_nbits(static_cast<unsigned char>(mantissa_nbits));
  py_params.replace(5, py_mantissa_nbits.to_robj());
}


/**
 *  .nepochs
 */
static GSArgs args_nepochs(
  "nepochs",
  "Number of epochs to train a model");


oobj Ftrl::get_nepochs() const {
  return py_params.get_attr("nepochs");
}


void Ftrl::set_nepochs(const Arg& py_nepochs) {
  size_t nepochs = py_nepochs.to_size_t();
  dtft->set_nepochs(nepochs);
  py_params.replace(6, py_nepochs.to_robj());
}


/**
 *  .double_precision
 */
static GSArgs args_double_precision(
  "double_precision",
  "Whether to use double precision arithmetic for modeling");


oobj Ftrl::get_double_precision() const {
  return py_params.get_attr("double_precision");
}

void Ftrl::set_double_precision(const Arg& py_double_precision) {
  if (dtft->is_trained()) {
    throw ValueError() << "Cannot change `double_precision` for a trained model, "
                       << "reset this model or create a new one";
  }
  double_precision = py_double_precision.to_bool_strict();
  py_params.replace(7, py_double_precision.to_robj());
}


/**
 *  .negative_class
 */
static GSArgs args_negative_class(
  "negative_class",
  "Whether to train on negatives in the case of multinomial classification.");


oobj Ftrl::get_negative_class() const {
  return py_params.get_attr("negative_class");
}


void Ftrl::set_negative_class(const Arg& py_negative_class) {
  if (dtft->is_trained()) {
    throw ValueError() << "Cannot change `negative_class` for a trained model, "
                       << "reset this model or create a new one";
  }
  bool negative_class = py_negative_class.to_bool_strict();
  dtft->set_negative_class(negative_class);
  py_params.replace(8, py_negative_class.to_robj());
}


/**
 *  .interactions
 */
static GSArgs args_interactions(
  "interactions",
  "List of feature lists to do interactions for");


oobj Ftrl::get_interactions() const {
  return py_params.get_attr("interactions");
}


void Ftrl::set_interactions(const Arg& arg_interactions) {
  if (dtft->is_trained())
    throw ValueError() << "Cannot change `interactions` for a trained model, "
                       << "reset this model or create a new one";

  auto py_interactions_in = arg_interactions.to_oiter();
  for (auto py_interaction : py_interactions_in) {
    if (!py_interaction.is_list())
      throw TypeError() << arg_interactions.name()
                        << " should be a list of lists, "
                        << "instead encountered: " << py_interaction;

    auto py_interaction_iter = py_interaction.to_oiter();
    if (!py_interaction_iter.size())
      throw TypeError() << "Interaction lists cannot be empty";

    for (auto py_feature : py_interaction_iter) {
      if (!py_feature.is_string())
        throw TypeError() << "Interaction features should be strings, "
                          << "instead encountered: " << py_feature;
    }
  }

  py_params.replace(9, arg_interactions.to_robj());
}



/**
 *  .params
 */
static GSArgs args_params(
  "params",
  "FTRL model parameters");


oobj Ftrl::get_params_namedtuple() const {
  return py_params;
}



void Ftrl::set_params_namedtuple(robj params_in) {
  py::otuple params_tuple = params_in.to_otuple();
  size_t n_params = params_tuple.size();
  if (n_params != 10) {
    throw ValueError() << "Tuple of FTRL parameters should have 10 elements, "
                       << "got: " << n_params;
  }
  py::oobj py_alpha = params_in.get_attr("alpha");
  py::oobj py_beta = params_in.get_attr("beta");
  py::oobj py_lambda1 = params_in.get_attr("lambda1");
  py::oobj py_lambda2 = params_in.get_attr("lambda2");
  py::oobj py_nbins = params_in.get_attr("nbins");
  py::oobj py_mantissa_nbits = params_in.get_attr("mantissa_nbits");
  py::oobj py_nepochs = params_in.get_attr("nepochs");
  py::oobj py_double_precision = params_in.get_attr("double_precision");
  py::oobj py_negative_class = params_in.get_attr("negative_class");
  py::oobj py_interactions = params_in.get_attr("interactions");

  set_alpha({py_alpha, "`FtrlParams.alpha`"});
  set_beta({py_beta, "`FtrlParams.beta`"});
  set_lambda1({py_lambda1, "`FtrlParams.lambda1`"});
  set_lambda2({py_lambda2, "`FtrlParams.lambda2`"});
  set_nbins({py_nbins, "`FtrlParams.nbins`"});
  set_mantissa_nbits({py_mantissa_nbits, "`FtrlParams.mantissa_nbits`"});
  set_nepochs({py_nepochs, "`FtrlParams.nepochs`"});
  set_double_precision({py_double_precision, "`FtrlParams.double_precision`"});
  set_negative_class({py_negative_class, "`FtrlParams.negative_class`"});
  set_interactions({py_interactions, "`FtrlParams.interactions`"});
}


oobj Ftrl::get_params_tuple() const {
  return otuple {get_alpha(),
                 get_beta(),
                 get_lambda1(),
                 get_lambda2(),
                 get_nbins(),
                 get_mantissa_nbits(),
                 get_nepochs(),
                 get_double_precision(),
                 get_negative_class(),
                 get_interactions()
                };
}


void Ftrl::set_params_tuple(robj params) {
  py::otuple params_tuple = params.to_otuple();
  size_t n_params = params_tuple.size();
  if (n_params != 10) {
    throw ValueError() << "Tuple of FTRL parameters should have 10 elements, "
                       << "got: " << n_params;
  }
  set_alpha({params_tuple[0], "alpha"});
  set_beta({params_tuple[1], "beta"});
  set_lambda1({params_tuple[2], "lambda1"});
  set_lambda2({params_tuple[3], "lambda2"});
  set_nbins({params_tuple[4], "nbins"});
  set_mantissa_nbits({params_tuple[5], "mantissa_nbits"});
  set_nepochs({params_tuple[6], "nepochs"});
  set_double_precision({params_tuple[7], "double_precision"});
  set_negative_class({params_tuple[8], "negative_class"});
  set_interactions({params_tuple[9], "interactions"});
}


/**
 *  Pickling support.
 */
static PKArgs args___getstate__(
    0, 0, 0, false, false, {}, "__getstate__", nullptr);


oobj Ftrl::m__getstate__(const PKArgs&) {
  py::oobj py_model = get_model();
  py::oobj py_fi = get_normalized_fi(false);
  py::oobj py_model_type = py::oint(static_cast<int32_t>(
                             dtft->get_model_type()
                           ));
  py::oobj py_labels = get_labels();
  py::oobj py_colnames = get_colnames();
  py::oobj py_params_tuple = get_params_tuple();

  return otuple {py_params_tuple, py_model, py_fi, py_model_type, py_labels,
                 py_colnames};
}


/**
 *  Unpickling support.
 */
static PKArgs args___setstate__(
    1, 0, 0, false, false, {"state"}, "__setstate__", nullptr);

void Ftrl::m__setstate__(const PKArgs& args) {
  m__dealloc__();
  dt::FtrlParams ftrl_params;

  py::otuple pickle = args[0].to_otuple();
  py::otuple py_params_tuple = pickle[0].to_otuple();

  double_precision = py_params_tuple[7].to_bool_strict();
  init_dt_ftrl();
  init_py_params();
  set_params_tuple(pickle[0]);
  set_model(pickle[1]);
  if (pickle[2].is_frame()) {
    dtft->set_fi(pickle[2].to_datatable()->copy());
  }

  dtft->set_model_type(static_cast<dt::FtrlModelType>(pickle[3].to_int32()));
  set_labels(pickle[4]);
  set_colnames(pickle[5]);
}


/**
 *  py::Ftrl::Type
 */
const char* Ftrl::Type::classname() {
  return "datatable.models.Ftrl";
}


const char* Ftrl::Type::classdoc() {
  return R"(Follow the Regularized Leader (FTRL) model.

FTRL model is a datatable implementation of the FTRL-Proximal online
learning algorithm for binomial logistic regression. It uses a hashing
trick for feature vectorization and the Hogwild approach
for parallelization. FTRL for multinomial classification and continuous
targets are implemented experimentally.

See this reference for more details:
https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf

Parameters
----------
alpha : float
    `alpha` in per-coordinate learning rate algorithm, defaults to `0.005`.

beta : float
    `beta` in per-coordinate learning rate algorithm, defaults to `1`.

lambda1 : float
    L1 regularization parameter, defaults to `0`.

lambda2 : float
    L2 regularization parameter, defaults to `0`.

nbins : int
    Number of bins to be used for the hashing trick, defaults to `10**6`.

mantissa_nbits : int
    Number of bits from mantissa to be used for hashing, defaults to `10`.

nepochs : int
    Number of training epochs, defaults to `1`.

double_precision : bool
    Whether to use double precision arithmetic or not, defaults to `False`.

negative_class : bool
    Whether to create and train on a "negative" class in the case of multinomial classification.
)";
}


/**
 *  Initialize all the exposed methods and getters/setters.
 */
void Ftrl::Type::init_methods_and_getsets(Methods& mm, GetSetters& gs)
{
  // Input parameters
  ADD_GETTER(gs, &Ftrl::get_params_namedtuple, args_params);
  ADD_GETSET(gs, &Ftrl::get_alpha, &Ftrl::set_alpha, args_alpha);
  ADD_GETSET(gs, &Ftrl::get_beta, &Ftrl::set_beta, args_beta);
  ADD_GETSET(gs, &Ftrl::get_lambda1, &Ftrl::set_lambda1, args_lambda1);
  ADD_GETSET(gs, &Ftrl::get_lambda2, &Ftrl::set_lambda2, args_lambda2);
  ADD_GETSET(gs, &Ftrl::get_nbins, &Ftrl::set_nbins, args_nbins);
  ADD_GETSET(gs, &Ftrl::get_mantissa_nbits, &Ftrl::set_mantissa_nbits, args_mantissa_nbits);
  ADD_GETSET(gs, &Ftrl::get_nepochs, &Ftrl::set_nepochs, args_nepochs);
  ADD_GETTER(gs, &Ftrl::get_double_precision, args_double_precision);
  ADD_GETSET(gs, &Ftrl::get_negative_class, &Ftrl::set_negative_class, args_negative_class);
  ADD_GETSET(gs, &Ftrl::get_interactions, &Ftrl::set_interactions, args_interactions);

  // Model and features
  ADD_GETTER(gs, &Ftrl::get_labels, args_labels);
  ADD_GETTER(gs, &Ftrl::get_model, args_model);
  ADD_GETTER(gs, &Ftrl::get_fi, args_fi);
  ADD_GETTER(gs, &Ftrl::get_colnames, args_colnames);
  ADD_GETTER(gs, &Ftrl::get_colname_hashes, args_colname_hashes);

  // Fit, predict and reset
  ADD_METHOD(mm, &Ftrl::fit, args_fit);
  ADD_METHOD(mm, &Ftrl::predict, args_predict);
  ADD_METHOD(mm, &Ftrl::reset, args_reset);

  // Pickling and unpickling
  ADD_METHOD(mm, &Ftrl::m__getstate__, args___getstate__);
  ADD_METHOD(mm, &Ftrl::m__setstate__, args___setstate__);
}


onamedtupletype Ftrl::py_ntt(
  "FtrlParams",
  args_params.doc, {
    {args_alpha.name,            args_alpha.doc},
    {args_beta.name,             args_beta.doc},
    {args_lambda1.name,          args_lambda1.doc},
    {args_lambda2.name,          args_lambda2.doc},
    {args_nbins.name,            args_nbins.doc},
    {args_mantissa_nbits.name,   args_mantissa_nbits.doc},
    {args_nepochs.name,          args_nepochs.doc},
    {args_double_precision.name, args_double_precision.doc},
    {args_negative_class.name,   args_negative_class.doc},
    {args_interactions.name,     args_interactions.doc}
  }
);


} // namespace py
