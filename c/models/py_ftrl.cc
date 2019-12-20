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

/**
 *  Model type names and their corresponding dt::FtrlModelType's
 */
static const std::unordered_map<std::string, dt::FtrlModelType> FtrlModelNameType {
   {"none", dt::FtrlModelType::NONE},
   {"auto", dt::FtrlModelType::AUTO},
   {"regression", dt::FtrlModelType::REGRESSION},
   {"binomial", dt::FtrlModelType::BINOMIAL},
   {"multinomial", dt::FtrlModelType::MULTINOMIAL}
};


/**
 *  Create and set inverse map for py::FtrlModelNameType
 */
static const std::map<dt::FtrlModelType, std::string> FtrlModelTypeName
  = Ftrl::create_model_type_name();

std::map<dt::FtrlModelType, std::string> Ftrl::create_model_type_name() {
  std::map<dt::FtrlModelType, std::string> m;
  for (const auto& v : py::FtrlModelNameType) {
    m.insert(std::pair<dt::FtrlModelType, std::string>(v.second, v.first));
  }
  return m;
}


static PKArgs args___init__(0, 1, 11, false, false,
                                 {"params", "alpha", "beta", "lambda1",
                                 "lambda2", "nbins", "mantissa_nbits",
                                 "nepochs", "double_precision",
                                 "negative_class", "interactions",
                                 "model_type"},
                                 "__init__", nullptr);


/**
 *  Ftrl(...)
 *  Initialize Ftrl object with the provided parameters.
 */
void Ftrl::m__init__(const PKArgs& args) {
  m__dealloc__();
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
  const Arg& arg_model_type       = args[11];


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
  bool defined_model_type       = !arg_model_type.is_none_or_undefined();
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
        << "`double_precision`, `negative_class`, `interactions` or `model_type` "
        << "to Ftrl constructor, but not both at the same time";
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
    if (defined_model_type) set_model_type(arg_model_type);
  }
}


void Ftrl::init_dt_ftrl() {
  delete dtft;
  if (double_precision) {
    dtft = new dt::Ftrl<double>();
  } else {
    dtft = new dt::Ftrl<float>();
  }
}


/**
 *  Deallocate underlying data for an Ftrl object.
 */
void Ftrl::m__dealloc__() {
  delete dtft;
  delete py_params;
  delete colnames;
  dtft = nullptr;
  py_params = nullptr;
  colnames = nullptr;
}


/**
 *  Check if provided interactions are consistent with the column names
 *  of the training frame. If yes, set up interactions for `dtft`.
 */
void Ftrl::init_dt_interactions() {
  std::vector<intvec> dt_interactions;
  auto py_interactions = py_params->get_attr("interactions").to_oiter();
  dt_interactions.reserve(py_interactions.size());

  for (auto py_interaction_robj : py_interactions) {
    intvec dt_interaction;
    auto py_interaction = py_interaction_robj.to_oiter();
    size_t nfeatures = py_interaction.size();
    dt_interaction.reserve(nfeatures);

    for (auto py_feature : py_interaction) {
      std::string feature_name = py_feature.to_string();

      auto it = find(colnames->begin(), colnames->end(), feature_name);
      if (it == colnames->end()) {
        throw ValueError() << "Feature `" << feature_name
          << "` is used in the interactions, however, column "
          << "`" << feature_name << "` is missing in the training frame";
      }

      auto feature_id = std::distance(colnames->begin(), it);
      dt_interaction.push_back(static_cast<size_t>(feature_id));
    }

    dt_interactions.push_back(std::move(dt_interaction));
  }
  dtft->set_interactions(std::move(dt_interactions));
}


/**
 *  .fit(...)
 *  Do dataset validation and a call to `dtft->dispatch_fit(...)` method.
 */
static PKArgs args_fit(2, 5, 0, false, false, {"X_train", "y_train",
                       "X_validation", "y_validation",
                       "nepochs_validation", "validation_error",
                       "validation_average_niterations"}, "fit",
R"(fit(self, X_train, y_train, X_validation=None, y_validation=None,
    nepochs_validation=1, validation_error=0.01,
    validation_average_niterations=1)
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

  if (!dtft->is_model_trained()) {
    delete colnames;
    colnames = new strvec(dt_X_train->get_names());
  }

  if (dtft->is_model_trained() && dt_X_train->get_names() != *colnames) {
    throw ValueError() << "Training frame names cannot change for a trained "
                       << "model";
  }

  if (!py_params->get_attr("interactions").is_none()
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

    if (dt_X_val->ncols() != dt_X_train->ncols()) {
      throw ValueError() << "Validation frame must have the same number of "
                         << "columns as the training frame";
    }

    if (dt_X_val->get_names() != *colnames) {
      throw ValueError() << "Validation frame must have the same column "
                         << "names as the training frame";
    }

    if (dt_X_val->nrows() == 0) {
      throw ValueError() << "Validation frame cannot be empty";
    }

    if (dt_y_val->ncols() != 1) {
      throw ValueError() << "Validation target frame must have exactly "
                         << "one column";
    }


    LType ltype = dt_y->get_column(0).ltype();
    LType ltype_val = dt_y_val->get_column(0).ltype();

    if (ltype != ltype_val) {
      throw TypeError() << "Training and validation target columns must have "
                        << "the same ltype, got: `" << info::ltype_name(ltype)
                        << "` and `" << info::ltype_name(ltype_val) << "`";
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
        static_cast<double>(dtft->get_nepochs()),
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

  dt::FtrlFitOutput output = dtft->dispatch_fit(dt_X_train, dt_y,
                                                dt_X_val, dt_y_val,
                                                nepochs_val, val_error, val_niters);

  static onamedtupletype py_fit_output_ntt(
    "FtrlFitOutput",
    "Tuple of fit output",
    {
      {"epoch", "epoch at which fitting stopped"},
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

  if (!dtft->is_model_trained()) {
    throw ValueError() << "Cannot make any predictions, the model "
                          "should be trained first";
  }

  size_t ncols = dtft->get_ncols();
  if (dt_X->ncols() != ncols && ncols != 0) {
    throw ValueError() << "Can only predict on a frame that has " << ncols
                       << " column" << (ncols == 1? "" : "s")
                       << ", i.e. has the same number of features as "
                          "was used for model training";
  }

  if (dt_X->get_names() != *colnames) {
    throw ValueError() << "Frames used for training and predictions "
                       << "should have the same column names";
  }

  if (!py_params->get_attr("interactions").is_none()
      && !dtft->get_interactions().size()) {
    init_dt_interactions();
  }


  if (!py_params->get_attr("interactions").is_none()
      && !dtft->get_interactions().size()) {
    init_dt_interactions();
  }


  DataTable* dt_p = dtft->predict(dt_X).release();
  py::oobj df_p = py::Frame::oframe(dt_p);

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
  if (colnames) colnames->clear();
}


/**
 *  .labels
 */
static GSArgs args_labels(
  "labels",
  R"(Frame of labels used for classification.)");


oobj Ftrl::get_labels() const {
  return dtft->get_labels();
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
  if (!dtft->is_model_trained()) return py::None();
  return dtft->get_model();
}


void Ftrl::set_model(robj model) {
  DataTable* dt_model = model.to_datatable();
  if (dt_model == nullptr) return;

  size_t ncols = dt_model->ncols();
  if (dt_model->nrows() != dtft->get_nbins() || dt_model->ncols()%2 != 0) {
    throw ValueError() << "Model frame must have " << dtft->get_nbins()
                       << " rows, and an even number of columns, "
                       << "whereas your frame has "
                       << dt_model->nrows() << " row"
                       << (dt_model->nrows() == 1? "": "s")
                       << " and "
                       << dt_model->ncols() << " column"
                       << (dt_model->ncols() == 1? "": "s");

  }

  SType stype = (double_precision)? SType::FLOAT64 : SType::FLOAT32;

  for (size_t i = 0; i < ncols; ++i) {

    const Column& col = dt_model->get_column(i);
    SType c_stype = col.stype();
    if (col.stype() != stype) {
      throw ValueError() << "Column " << i << " in the model frame should "
                         << "have a type of " << stype << ", whereas it has "
                         << "the following type: "
                         << c_stype;
    }

    if ((i % 2) && py::Validator::has_negatives(col)) {
      throw ValueError() << "Column " << i << " cannot have negative values";
    }
  }
  dtft->set_model(*dt_model);
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
  if (!dtft->is_model_trained()) return py::None();
  return dtft->get_fi(normalize);
}


/**
 *  .colnames
 */
static GSArgs args_colnames(
  "colnames",
  "Column names."
);


oobj Ftrl::get_colnames() const {
  if (dtft->is_model_trained()) {
    size_t ncols = colnames->size();
    py::olist py_colnames(ncols);
    for (size_t i = 0; i < ncols; ++i) {
      py_colnames.set(i, py::ostring((*colnames)[i]));
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

    delete colnames;
    colnames = new strvec();
    colnames->reserve(ncolnames);
    for (size_t i = 0; i < ncolnames; ++i) {
      colnames->push_back(py_colnames_list[i].to_string());
    }
  }
}


/**
 *  .colname_hashes
 */
static GSArgs args_colname_hashes(
  "colname_hashes",
  "Column name hashes."
);


oobj Ftrl::get_colname_hashes() const {
  if (dtft->is_model_trained()) {
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
  "`alpha` in per-coordinate learning rate formula.");


oobj Ftrl::get_alpha() const {
  return py_params->get_attr("alpha");
}


void Ftrl::set_alpha(const Arg& py_alpha) {
  double alpha = py_alpha.to_double();
  py::Validator::check_finite(alpha, py_alpha);
  py::Validator::check_positive(alpha, py_alpha);
  dtft->set_alpha(alpha);
  py_params->replace(0, py_alpha.robj());
}


/**
 *  .beta
 */
static GSArgs args_beta(
  "beta",
  "`beta` in per-coordinate learning rate formula.");


oobj Ftrl::get_beta() const {
  return py_params->get_attr("beta");
}


void Ftrl::set_beta(const Arg& py_beta) {
  double beta = py_beta.to_double();
  py::Validator::check_finite(beta, py_beta);
  py::Validator::check_not_negative(beta, py_beta);
  dtft->set_beta(beta);
  py_params->replace(1, py_beta.to_robj());
}


/**
 *  .lambda1
 */
static GSArgs args_lambda1(
  "lambda1",
  "L1 regularization parameter.");


oobj Ftrl::get_lambda1() const {
  return py_params->get_attr("lambda1");
}


void Ftrl::set_lambda1(const Arg& py_lambda1) {
  double lambda1 = py_lambda1.to_double();
  py::Validator::check_finite(lambda1, py_lambda1);
  py::Validator::check_not_negative(lambda1, py_lambda1);
  dtft->set_lambda1(lambda1);
  py_params->replace(2, py_lambda1.to_robj());
}


/**
 *  .lambda2
 */
static GSArgs args_lambda2(
  "lambda2",
  "L2 regularization parameter.");


oobj Ftrl::get_lambda2() const {
  return py_params->get_attr("lambda2");
}


void Ftrl::set_lambda2(const Arg& py_lambda2) {
  double lambda2 = py_lambda2.to_double();
  py::Validator::check_finite(lambda2, py_lambda2);
  py::Validator::check_not_negative(lambda2, py_lambda2);
  dtft->set_lambda2(lambda2);
  py_params->replace(3, py_lambda2.to_robj());
}


/**
 *  .nbins
 */
static GSArgs args_nbins(
  "nbins",
  "Number of bins to be used for the hashing trick.");


oobj Ftrl::get_nbins() const {
  return py_params->get_attr("nbins");
}


void Ftrl::set_nbins(const Arg& arg_nbins) {
  if (dtft->is_model_trained()) {
    throw ValueError() << "Cannot change " << arg_nbins.name()
                       << " for a trained model, "
                       << "reset this model or create a new one";
  }

  size_t nbins = arg_nbins.to_size_t();
  py::Validator::check_positive(nbins, arg_nbins);
  dtft->set_nbins(static_cast<uint64_t>(nbins));
  py_params->replace(4, arg_nbins.to_robj());
}


/**
 *  .mantissa_nbits
 */
static GSArgs args_mantissa_nbits(
  "mantissa_nbits",
  "Number of bits from mantissa to be used for hashing floats.");


oobj Ftrl::get_mantissa_nbits() const {
  return py_params->get_attr("mantissa_nbits");
}


void Ftrl::set_mantissa_nbits(const Arg& arg_mantissa_nbits) {
  if (dtft->is_model_trained()) {

    throw ValueError() << "Cannot change " << arg_mantissa_nbits.name()
                       << " for a trained model, "
                       << "reset this model or create a new one";
  }

  size_t mantissa_nbits = arg_mantissa_nbits.to_size_t();
  py::Validator::check_less_than_or_equal_to<uint64_t>(
    mantissa_nbits,
    dt::FtrlBase::DOUBLE_MANTISSA_NBITS,
    arg_mantissa_nbits
  );
  dtft->set_mantissa_nbits(static_cast<unsigned char>(mantissa_nbits));
  py_params->replace(5, arg_mantissa_nbits.to_robj());
}


/**
 *  .nepochs
 */
static GSArgs args_nepochs(
  "nepochs",
  "Number of training epochs.");


oobj Ftrl::get_nepochs() const {
  return py_params->get_attr("nepochs");
}


void Ftrl::set_nepochs(const Arg& py_nepochs) {
  size_t nepochs = py_nepochs.to_size_t();
  dtft->set_nepochs(nepochs);
  py_params->replace(6, py_nepochs.to_robj());
}


/**
 *  .double_precision
 */
static GSArgs args_double_precision(
  "double_precision",
  "Whether to use double precision arithmetic or not.");


oobj Ftrl::get_double_precision() const {
  return py_params->get_attr("double_precision");
}

void Ftrl::set_double_precision(const Arg& arg_double_precision) {
  if (dtft->is_model_trained()) {
    throw ValueError() << "Cannot change " << arg_double_precision.name()
                       << "for a trained model, "
                       << "reset this model or create a new one";
  }
  double_precision = arg_double_precision.to_bool_strict();
  py_params->replace(7, arg_double_precision.to_robj());
}


/**
 *  .negative_class
 */
static GSArgs args_negative_class(
  "negative_class",
R"(Whether to create and train on a 'negative' class in the case of
multinomial classification.)");


oobj Ftrl::get_negative_class() const {
  return py_params->get_attr("negative_class");
}


void Ftrl::set_negative_class(const Arg& arg_negative_class) {
  if (dtft->is_model_trained()) {
    throw ValueError() << "Cannot change " << arg_negative_class.name()
                       << " for a trained model, "
                       << "reset this model or create a new one";
  }
  bool negative_class = arg_negative_class.to_bool_strict();
  dtft->set_negative_class(negative_class);
  py_params->replace(8, arg_negative_class.to_robj());
}


/**
 *  .interactions
 */
static GSArgs args_interactions(
  "interactions",
R"(A list or a tuple of interactions. In turn, each interaction
should be a list or a tuple of feature names, where each feature
name is a column name from the training frame.)");


oobj Ftrl::get_interactions() const {
  return py_params->get_attr("interactions");
}


void Ftrl::set_interactions(const Arg& arg_interactions) {
  if (dtft->is_model_trained())
    throw ValueError() << "Cannot change " << arg_interactions.name()
                       << " for a trained model, reset this model or"
                       << " create a new one";

  if (arg_interactions.is_none()) {
    py_params->replace(9, arg_interactions.to_robj());
    return;
  }

  if (!arg_interactions.is_list() && !arg_interactions.is_tuple())
    throw TypeError() << arg_interactions.name() << " should be a "
                      << "list or a tuple, instead got: "
                      << arg_interactions.typeobj();

  // Convert the input into a tuple of tuples.
  auto py_interactions = arg_interactions.to_oiter();
  py::otuple params_interactions(py_interactions.size());
  size_t i = 0;

  for (auto py_interaction_robj : py_interactions) {
    if (!py_interaction_robj.is_list() && !py_interaction_robj.is_tuple())
      throw TypeError() << arg_interactions.name()
                        << " should be a list or a tuple of lists or tuples, "
                        << "instead encountered: " << py_interaction_robj;

    auto py_interaction = py_interaction_robj.to_oiter();
    if (!py_interaction.size())
      throw TypeError() << "Interaction cannot have zero features, encountered: "
                        << py_interaction_robj;

    py::otuple params_interaction(py_interaction.size());
    size_t j = 0;

    for (auto py_feature_robj : py_interaction) {
      if (!py_feature_robj.is_string())
        throw TypeError() << "Interaction features should be strings, "
                          << "instead encountered: " << py_feature_robj;
      params_interaction.set(j++, py::oobj(py_feature_robj));
    }

    params_interactions.set(i++, std::move(params_interaction));
  }

  py_params->replace(9, std::move(params_interactions));
}


/**
 *  .model_type
 */
static GSArgs args_model_type(
  "model_type",
R"(The type of the model FTRL should build: 'binomial' for binomial
classification, 'multinomial' for multinomial classification,
'regression' for numeric regression or 'auto' for automatic
model type detection based on the target column `stype`.
Default value is 'auto'.)");


oobj Ftrl::get_model_type() const {
  return py_params->get_attr("model_type");
}


void Ftrl::set_model_type(const Arg& py_model_type) {
  if (dtft->is_model_trained()) {
    throw ValueError() << "Cannot change `model_type` for a trained model, "
                       << "reset this model or create a new one";
  }
  std::string model_type = py_model_type.to_string();
  auto it = py::FtrlModelNameType.find(model_type);
  if (it == py::FtrlModelNameType.end() || it->second == dt::FtrlModelType::NONE) {
    throw ValueError() << "Model type `" << model_type << "` is not supported";
  }

  dtft->set_model_type(it->second);
  py_params->replace(10, py_model_type.to_robj());
}


/**
 *  .model_type_trained
 */
static GSArgs args_model_type_trained(
  "model_type_trained",
R"(The model type FTRL has built: 'regression', 'binomial', 'multinomial'
or 'none' for untrained model.)");


oobj Ftrl::get_model_type_trained() const {
  dt::FtrlModelType dt_model_type = dtft->get_model_type_trained();
  std::string model_type = FtrlModelTypeName.at(dt_model_type);
  return py::ostring(std::move(model_type));
}


/**
 *  .params
 */
static GSArgs args_params(
  "params",
  "FTRL model parameters.");


oobj Ftrl::get_params_namedtuple() const {
  return *py_params;
}



void Ftrl::set_params_namedtuple(robj params_in) {
  py::otuple params_tuple = params_in.to_otuple();
  size_t n_params = params_tuple.size();

  if (n_params != 11) {
    throw ValueError() << "Tuple of FTRL parameters should have 11 elements, "
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
  py::oobj py_model_type = params_in.get_attr("model_type");


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
  set_model_type({py_model_type, "`FtrlParams.model_type`"});
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
                 get_interactions(),
                 get_model_type()
                };
}


void Ftrl::set_params_tuple(robj params) {
  py::otuple params_tuple = params.to_otuple();
  size_t n_params = params_tuple.size();
  if (n_params != 11) {
    throw ValueError() << "Tuple of FTRL parameters should have 11 elements, "
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
  set_model_type({params_tuple[10], "model_type"});
}


void Ftrl::init_py_params() {
  static onamedtupletype py_params_ntt(
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
      {args_interactions.name,     args_interactions.doc},
      {args_model_type.name,       args_model_type.doc}
    }
  );

  dt::FtrlParams params;
  delete py_params;
  py_params = new py::onamedtuple(py_params_ntt);

  py_params->replace(0, py::ofloat(params.alpha));
  py_params->replace(1, py::ofloat(params.beta));
  py_params->replace(2, py::ofloat(params.lambda1));
  py_params->replace(3, py::ofloat(params.lambda2));
  py_params->replace(4, py::oint(static_cast<size_t>(params.nbins)));
  py_params->replace(5, py::oint(params.mantissa_nbits));
  py_params->replace(6, py::oint(params.nepochs));
  py_params->replace(7, py::obool(params.double_precision));
  py_params->replace(8, py::obool(params.negative_class));
  py_params->replace(9, py::None());
  py_params->replace(10, py::ostring("auto"));
}



/**
 *  Pickling support.
 */
static PKArgs args___getstate__(
    0, 0, 0, false, false, {}, "__getstate__", nullptr);


oobj Ftrl::m__getstate__(const PKArgs&) {
  py::oobj py_api_version = py::oint(API_VERSION);
  py::oobj py_model = get_model();
  py::oobj py_fi = get_normalized_fi(false);
  py::oobj py_labels = get_labels();
  py::oobj py_colnames = get_colnames();
  py::oobj py_params_tuple = get_params_tuple();
  py::oobj py_model_type = get_model_type_trained();

  return otuple {py_api_version,
                 py_params_tuple,
                 py_model,
                 py_fi,
                 py_labels,
                 py_colnames,
                 py_model_type
                };
}


/**
 *  Unpickling support.
 */
static PKArgs args___setstate__(
    1, 0, 0, false, false, {"state"}, "__setstate__", nullptr);

void Ftrl::m__setstate__(const PKArgs& args) {
  py::otuple pickle = args[0].to_otuple();

  if (!pickle[0].is_int()) {
    throw TypeError() << "This FTRL model was pickled with the old "
                      << "version of datatable, that has no information "
                      << "on the FTRL API version";
  }

  py::oint py_api_version = pickle[0].to_size_t(); // Not used for the moment
  py::otuple py_params_tuple = pickle[1].to_otuple();

  double_precision = py_params_tuple[7].to_bool_strict();
  init_dt_ftrl();
  init_py_params();
  set_params_tuple(pickle[1]);
  set_model(pickle[2]);
  if (pickle[3].is_frame()) {
    dtft->set_fi(*pickle[3].to_datatable());
  }

  if (pickle[4].is_frame()) {
    dtft->set_labels(*pickle[4].to_datatable());
  }
  set_colnames(pickle[5]);

  auto model_type = py::FtrlModelNameType.at(pickle[6].to_string());
  dtft->set_model_type_trained(model_type);
}




//------------------------------------------------------------------------------
// py::Ftrl::Type
//------------------------------------------------------------------------------

void Ftrl::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.models.Ftrl");
  xt.set_class_doc(R"(Follow the Regularized Leader (FTRL) model.

FTRL model is a datatable implementation of the FTRL-Proximal online
learning algorithm for binomial logistic regression. It uses a hashing
trick for feature vectorization and the Hogwild approach
for parallelization. Multinomial classification and regression for
continuous targets are implemented experimentally.

See this reference for more details:
https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf

Parameters
----------
alpha : float
    `alpha` in per-coordinate learning rate formula, defaults to `0.005`.

beta : float
    `beta` in per-coordinate learning rate formula, defaults to `1`.

lambda1 : float
    L1 regularization parameter, defaults to `0`.

lambda2 : float
    L2 regularization parameter, defaults to `0`.

nbins : int
    Number of bins to be used for the hashing trick, defaults to `10**6`.

mantissa_nbits : int
    Number of bits from mantissa to be used for hashing floats,
    defaults to `10`.

nepochs : int
    Number of training epochs, defaults to `1`.

double_precision : bool
    Whether to use double precision arithmetic or not, defaults to `False`.

negative_class : bool
    Whether to create and train on a 'negative' class in the case of
    multinomial classification.

interactions : list or tuple
    A list or a tuple of interactions. In turn, each interaction
    should be a list or a tuple of feature names, where each feature
    name is a column name from the training frame.

model_type : str
    Model type can be one of the following: 'binomial' for binomial
    classification, 'multinomial' for multinomial classification, and
    'regression' for numeric regression. Defaults to 'auto', meaning
    that the model type will be automatically selected based on
    the target column `stype`.
)");

  xt.add(CONSTRUCTOR(&Ftrl::m__init__, args___init__));
  xt.add(DESTRUCTOR(&Ftrl::m__dealloc__));

  // Input parameters
  xt.add(GETTER(&Ftrl::get_params_namedtuple, args_params));
  xt.add(GETSET(&Ftrl::get_alpha, &Ftrl::set_alpha, args_alpha));
  xt.add(GETSET(&Ftrl::get_beta, &Ftrl::set_beta, args_beta));
  xt.add(GETSET(&Ftrl::get_lambda1, &Ftrl::set_lambda1, args_lambda1));
  xt.add(GETSET(&Ftrl::get_lambda2, &Ftrl::set_lambda2, args_lambda2));
  xt.add(GETSET(&Ftrl::get_nbins, &Ftrl::set_nbins, args_nbins));
  xt.add(GETSET(&Ftrl::get_mantissa_nbits, &Ftrl::set_mantissa_nbits, args_mantissa_nbits));
  xt.add(GETSET(&Ftrl::get_nepochs, &Ftrl::set_nepochs, args_nepochs));
  xt.add(GETTER(&Ftrl::get_double_precision, args_double_precision));
  xt.add(GETSET(&Ftrl::get_negative_class, &Ftrl::set_negative_class, args_negative_class));
  xt.add(GETSET(&Ftrl::get_interactions, &Ftrl::set_interactions, args_interactions));
  xt.add(GETSET(&Ftrl::get_model_type, &Ftrl::set_model_type, args_model_type));

  // Model and features
  xt.add(GETTER(&Ftrl::get_labels, args_labels));
  xt.add(GETTER(&Ftrl::get_model_type_trained, args_model_type_trained));
  xt.add(GETTER(&Ftrl::get_model, args_model));
  xt.add(GETTER(&Ftrl::get_fi, args_fi));
  xt.add(GETTER(&Ftrl::get_colnames, args_colnames));
  xt.add(GETTER(&Ftrl::get_colname_hashes, args_colname_hashes));

  // Fit, predict and reset
  xt.add(METHOD(&Ftrl::fit, args_fit));
  xt.add(METHOD(&Ftrl::predict, args_predict));
  xt.add(METHOD(&Ftrl::reset, args_reset));

  // Pickling and unpickling
  xt.add(METHOD(&Ftrl::m__getstate__, args___getstate__));
  xt.add(METHOD(&Ftrl::m__setstate__, args___setstate__));
}




} // namespace py
