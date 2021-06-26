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
#include <iterator>        // std::distance
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


/**
 *  Ftrl(...)
 *  Initialize Ftrl object with the provided parameters.
 */

static const char* doc___init__ =
R"(__init__(self, alpha=0.005, beta=1, lambda1=0, lambda2=0, nbins=10**6,
mantissa_nbits=10, nepochs=1, double_precision=False, negative_class=False,
interactions=None, model_type='auto', params=None)
--

Create a new :class:`Ftrl <datatable.models.Ftrl>` object.

Parameters
----------
alpha: float
    :math:`\alpha` in per-coordinate FTRL-Proximal algorithm, should be
    positive.

beta: float
    :math:`\beta` in per-coordinate FTRL-Proximal algorithm, should be non-negative.

lambda1: float
    L1 regularization parameter, :math:`\lambda_1` in per-coordinate
    FTRL-Proximal algorithm. It should be non-negative.

lambda2: float
    L2 regularization parameter, :math:`\lambda_2` in per-coordinate
    FTRL-Proximal algorithm. It should be non-negative.

nbins: int
    Number of bins to be used for the hashing trick, should be positive.

mantissa_nbits: int
    Number of mantissa bits to take into account when hashing floats.
    It should be non-negative and less than or equal to `52`, that
    is a number of mantissa bits allocated for a C++ 64-bit `double`.

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
    footprint of the `Ftrl` object.

negative_class: bool
    An option to indicate if a "negative" class should be created
    in the case of multinomial classification. For the "negative"
    class the model will train on all the negatives, and if
    a new label is encountered in the target column, its
    weights will be initialized to the current "negative" class weights.
    If `negative_class` is set to `False`, the initial weights
    become zeros.

interactions: List[List[str] | Tuple[str]] | Tuple[List[str] | Tuple[str]]
    A list or a tuple of interactions. In turn, each interaction
    should be a list or a tuple of feature names, where each feature
    name is a column name from the training frame. Each interaction
    should have at least one feature.

model_type: "binomial" | "multinomial" | "regression" | "auto"
    The model type to be built. When this option is `"auto"`
    then the model type will be automatically chosen based on
    the target column `stype`.

params: FtrlParams
    Named tuple of the above parameters. One can pass either this tuple,
    or any combination of the individual parameters to the constructor,
    but not both at the same time.

except: ValueError
    The exception is raised if both the `params` and one of the
    individual model parameters are passed at the same time.

)";

static PKArgs args___init__(0, 1, 11, false, false,
                                 {"params", "alpha", "beta", "lambda1",
                                 "lambda2", "nbins", "mantissa_nbits",
                                 "nepochs", "double_precision",
                                 "negative_class", "interactions",
                                 "model_type"},
                                 "__init__", doc___init__);



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
      throw ValueError() << "You can either pass all the parameters with "
        << "`params` or any of the individual parameters with `alpha`, "
        << "`beta`, `lambda1`, `lambda2`, `nbins`, `mantissa_nbits`, `nepochs`, "
        << "`double_precision`, `negative_class`, `interactions` or `model_type` "
        << "to `Ftrl` constructor, but not both at the same time";
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
  std::vector<sztvec> dt_interactions;
  auto py_interactions = py_params->get_attr("interactions").to_oiter();
  dt_interactions.reserve(py_interactions.size());

  for (auto py_interaction_robj : py_interactions) {
    sztvec dt_interaction;
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

static const char* doc_fit =
R"(fit(self, X_train, y_train, X_validation=None, y_validation=None,
    nepochs_validation=1, validation_error=0.01,
    validation_average_niterations=1)
--

Train model on the input samples and targets.

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

return: FtrlFitOutput
    `FtrlFitOutput` is a `Tuple[float, float]` with two fields: `epoch` and `loss`,
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

    dt::LType ltype, ltype_val;

    for (size_t i = 0; i < dt_X_train->ncols(); i++) {
      ltype = dt_X_train->get_column(i).ltype();
      ltype_val = dt_X_val->get_column(i).ltype();

      if (ltype != ltype_val) {
        throw TypeError() << "Training and validation frames must have "
                          << "identical column ltypes, instead for a column `"
                          << (*colnames)[i] << "`, got ltypes: `" << ltype << "` and `"
                          << ltype_val << "`";
      }
    }

    if (dt_X_val->nrows() == 0) {
      throw ValueError() << "Validation frame cannot be empty";
    }

    if (dt_y_val->ncols() != 1) {
      throw ValueError() << "Validation target frame must have exactly "
                         << "one column";
    }


    ltype = dt_y->get_column(0).ltype();
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
        dtft->get_nepochs(),
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
 *  Perform dataset validation, make a call to `dtft->predict(...)`,
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


  DataTable* dt_p = dtft->predict(dt_X).release();
  py::oobj df_p = py::Frame::oframe(dt_p);

  return df_p;
}


/**
 *  .reset()
 *  Reset the model by making a call to `dtft->reset()`.
 */

static const char* doc_reset =
R"(reset(self)
--

Reset `Ftrl` model by resetting all the model weights, labels and
feature importance information.

Parameters
----------
return: None

See also
--------
- :meth:`.fit` -- train model on a dataset.
- :meth:`.predict` -- predict on a dataset.

)";

static PKArgs args_reset(0, 0, 0, false, false, {}, "reset", doc_reset);


void Ftrl::reset(const PKArgs&) {
  dtft->reset();
  if (colnames) colnames->clear();
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


oobj Ftrl::get_labels() const {
  return dtft->get_labels();
}


/**
 *  .model
 */

static const char* doc_model =
R"(
Trained models weights, i.e. `z` and `n` coefficients
in per-coordinate FTRL-Proximal algorithm.

Parameters
----------
return: Frame
    A frame of shape `(nbins, 2 * nlabels)`, where `nlabels` is
    the total number of labels the model was trained on, and
    :attr:`nbins <datatable.models.Ftrl.nbins>` is the number of bins
    used for the hashing trick. Odd and even columns represent
    the `z` and `n` model coefficients, respectively.
)";

static GSArgs args_model("model", doc_model);

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

  auto stype = double_precision? dt::SType::FLOAT64 : dt::SType::FLOAT32;

  for (size_t i = 0; i < ncols; ++i) {

    const Column& col = dt_model->get_column(i);
    dt::SType c_stype = col.stype();
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

static const char* doc_fi =
R"(
Feature importances as calculated during the model training and
normalized to `[0; 1]`. The normalization is done by dividing
the accumulated feature importances over the maximum value.

Parameters
----------
return: Frame
    A frame with two columns: `feature_name` that has stype `str32`,
    and `feature_importance` that has stype `float32` or `float64`
    depending on whether the :attr:`.double_precision`
    option is `False` or `True`.
)";

static GSArgs args_fi(
  "feature_importances",
  doc_fi
);


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

static const char* doc_colnames =
R"(
Column names of the training frame, i.e. the feature names.

Parameters
----------
return: List[str]
    A list of the column names.

See also
--------
- :attr:`.colname_hashes` -- the hashed column names.

)";

static GSArgs args_colnames(
  "colnames",
  doc_colnames
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

static const char* doc_colname_hashes =
R"(
Hashes of the column names used for the hashing trick as
described in the :class:`Ftrl <dt.models.Ftrl>` class description.

Parameters
----------
return: List[int]
    A list of the column name hashes.

See also
--------
- :attr:`.colnames` -- the column names of the
  training frame, i.e. the feature names.

)";

static GSArgs args_colname_hashes(
  "colname_hashes",
  doc_colname_hashes
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

static const char* doc_alpha =
R"(
:math:`\alpha` in per-coordinate FTRL-Proximal algorithm.

Parameters
----------
return: float
    Current `alpha` value.

new_alpha: float
    New `alpha` value, should be positive.

except: ValueError
    The exception is raised when `new_alpha` is not positive.
)";

static GSArgs args_alpha(
  "alpha",
  doc_alpha
);


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

static const char* doc_beta =
R"(
:math:`\beta` in per-coordinate FTRL-Proximal algorithm.

Parameters
----------
return: float
    Current `beta` value.

new_beta: float
    New `beta` value, should be non-negative.

except: ValueError
    The exception is raised when `new_beta` is negative.

)";

static GSArgs args_beta(
  "beta",
  doc_beta
);


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

static const char* doc_lambda1 =
R"(
L1 regularization parameter, :math:`\lambda_1` in per-coordinate
FTRL-Proximal algorithm.

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

static const char* doc_lambda2 =
R"(
L2 regularization parameter, :math:`\lambda_2` in per-coordinate
FTRL-Proximal algorithm.

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

static const char* doc_nbins =
R"(
Number of bins to be used for the hashing trick.
This option is read-only for a trained model.

Parameters
----------
return: int
    Current `nbins` value.

new_nbins: int
    New `nbins` value, should be positive.

except: ValueError
    The exception is raised when

    - trying to change this option for a model that has already been trained;
    - `new_nbins` value is not positive.

)";

static GSArgs args_nbins(
  "nbins",
  doc_nbins
);


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

static const char* doc_mantissa_nbits =
R"(
Number of mantissa bits to take into account for hashing floats.
This option is read-only for a trained model.

Parameters
----------
return: int
    Current `mantissa_nbits` value.

new_mantissa_nbits: int
    New `mantissa_nbits` value, should be non-negative and
    less than or equal to `52`, that is a number of
    mantissa bits in a C++ 64-bit `double`.

except: ValueError
    The exception is raised when

    - trying to change this option for a model that has already been trained;
    - `new_mantissa_nbits` value is negative or larger than `52`.

)";

static GSArgs args_mantissa_nbits(
  "mantissa_nbits",
  doc_mantissa_nbits
);


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

oobj Ftrl::get_nepochs() const {
  return py_params->get_attr("nepochs");
}


void Ftrl::set_nepochs(const Arg& arg_nepochs) {
  double nepochs = arg_nepochs.to_double();
  py::Validator::check_finite(nepochs, arg_nepochs);
  py::Validator::check_not_negative(nepochs, arg_nepochs);
  dtft->set_nepochs(nepochs);
  py_params->replace(6, arg_nepochs.to_robj());
}


/**
 *  .double_precision
 */

static const char* doc_double_precision =
R"(
An option to indicate whether double precision, i.e. `float64`,
or single precision, i.e. `float32`, arithmetic should be
used for computations. This option is read-only and can only be set
during the `Ftrl` object :meth:`construction <datatable.models.Ftrl.__init__>`.

Parameters
----------
return: bool
    Current `double_precision` value.

)";

static GSArgs args_double_precision(
  "double_precision",
  doc_double_precision
);


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

static const char* doc_negative_class =
R"(
An option to indicate if a "negative" class should be created
in the case of multinomial classification. For the "negative"
class the model will train on all the negatives, and if
a new label is encountered in the target column, its
weights are initialized to the current "negative" class weights.
If `negative_class` is set to `False`, the initial weights
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

static const char* doc_interactions =
R"(
The feature interactions to be used for model training. This option is
read-only for a trained model.

Parameters
----------
return: Tuple
    Current `interactions` value.

new_interactions: List[List[str] | Tuple[str]] | Tuple[List[str] | Tuple[str]]
    New `interactions` value. Each particular interaction
    should be a list or a tuple of feature names, where each feature
    name is a column name from the training frame.

except: ValueError
    The exception is raised when

    - trying to change this option for a model that has already been trained;
    - one of the interactions has zero features.

)";

static GSArgs args_interactions(
  "interactions",
  doc_interactions
);


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
      throw ValueError() << "Interaction cannot have zero features, encountered: "
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

static const char* doc_model_type =
R"(
A type of the model `Ftrl` should build:

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
    The exception is raised when

    - trying to change this option for a model that has already been trained;
    - `new_model_type` value is not one of the following: `"binomial"`,
      `"multinomial"`, `"regression"` or `"auto"`.

See also
--------
- :attr:`.model_type_trained` -- the model type `Ftrl` has build.
)";

static GSArgs args_model_type(
  "model_type",
  doc_model_type
);


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

static const char* doc_model_type_trained =
R"(
The model type `Ftrl` has built.

Parameters
----------
return: str
    Could be one of the following: `"regression"`, `"binomial"`,
    `"multinomial"` or `"none"` for untrained model.

See also
--------
- :attr:`.model_type` -- the model type `Ftrl` should build.
)";

static GSArgs args_model_type_trained(
  "model_type_trained",
  doc_model_type_trained
);


oobj Ftrl::get_model_type_trained() const {
  dt::FtrlModelType dt_model_type = dtft->get_model_type_trained();
  std::string model_type = FtrlModelTypeName.at(dt_model_type);
  return py::ostring(std::move(model_type));
}


/**
 *  .params
 */

static const char* doc_params =
R"(
`Ftrl` model parameters as a named tuple `FtrlParams`,
see :meth:`.__init__` for more details.
This option is read-only for a trained model.

Parameters
----------
return: FtrlParams
    Current `params` value.

new_params: FtrlParams
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
  py_params->replace(6, py::ofloat(params.nepochs));
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

static const char* doc_Ftrl =
R"(
This class implements the Follow the Regularized Leader (FTRL) model,
that is based on the
`FTRL-Proximal <https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf>`_
online learning algorithm for binomial logistic regression. Multinomial
classification and regression for continuous targets are also implemented,
though these implementations are experimental. This model is fully parallel
and is based on the
`Hogwild approach <https://people.eecs.berkeley.edu/~brecht/papers/hogwildTR.pdf>`_
for parallelization.

The model supports numerical (boolean, integer and float types),
temporal (date and time types) and string features. To vectorize features a hashing trick
is employed, such that all the values are hashed with the 64-bit hashing function.
This function is implemented as follows:

- for booleans and integers the hashing function is essentially an identity
  function;

- for floats the hashing function trims mantissa, taking into account
  :attr:`mantissa_nbits <datatable.models.Ftrl.mantissa_nbits>`,
  and interprets the resulting bit representation as a 64-bit unsigned integer;

- for date and time types the hashing function is essentially an identity
  function that is based on their internal integer representations;

- for strings the 64-bit `Murmur2 <https://github.com/aappleby/smhasher>`_
  hashing function is used.

To compute the final hash `x` the Murmur2 hashed feature name is added
to the hashed feature and the result is modulo divided by the number of
requested bins, i.e. by :attr:`nbins <datatable.models.Ftrl.nbins>`.

For each hashed row of data, according to
`Ad Click Prediction: a View from the Trenches <https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf>`_,
the following FTRL-Proximal algorithm is employed:

.. raw:: html

      <img src="../../_static/ftrl_algorithm.png" width="400"
       alt="Per-coordinate FTRL-Proximal online learning algorithm" />

When trained, the model can be used to make predictions, or it can be
re-trained on new datasets as many times as needed improving
model weights from run to run.

)";

void Ftrl::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.models.Ftrl");
  xt.set_class_doc(doc_Ftrl);

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
