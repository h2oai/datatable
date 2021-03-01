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
#include <iterator>        // std::distance
#include <vector>
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/string.h"
#include "str/py_str.h"
#include "models/py_linearmodel.h"
#include "models/py_validator.h"
#include "models/utils.h"

namespace py {

/**
 *  Model type names and their corresponding dt::LinearModelType's
 */
static const std::unordered_map<std::string, dt::LinearModelType> LinearModelNameType {
   {"none", dt::LinearModelType::NONE},
   {"auto", dt::LinearModelType::AUTO},
   {"regression", dt::LinearModelType::REGRESSION},
   {"binomial", dt::LinearModelType::BINOMIAL},
   {"multinomial", dt::LinearModelType::MULTINOMIAL}
};


/**
 *  Create and set inverse map for py::LinearModelNameType
 */
static const std::map<dt::LinearModelType, std::string> LinearModelTypeName
  = LinearModel::create_model_type_name();

std::map<dt::LinearModelType, std::string> LinearModel::create_model_type_name() {
  std::map<dt::LinearModelType, std::string> m;
  for (const auto& v : py::LinearModelNameType) {
    m.insert(std::pair<dt::LinearModelType, std::string>(v.second, v.first));
  }
  return m;
}


/**
 *  LinearModel(...)
 *  Initialize LinearModel object with the provided parameters.
 */

static const char* doc___init__ =
R"(__init__(self, eta=0.005, lambda1=0, lambda2=0,
nepochs=1, double_precision=False, negative_class=False,
model_type='auto', params=None)
--

Create a new :class:`LinearModel <datatable.models.LinearModel>` object.

Parameters
----------
eta: float
    :math:`\eta` step size aka learning rate.

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
    weights will be initialized to the current "negative" class weights.
    If `negative_class` is set to `False`, the initial weights
    become zeros.

model_type: "binomial" | "multinomial" | "regression" | "auto"
    The model type to be built. When this option is `"auto"`
    then the model type will be automatically chosen based on
    the target column `stype`.

params: LinearModelParams
    Named tuple of the above parameters. One can pass either this tuple,
    or any combination of the individual parameters to the constructor,
    but not both at the same time.

except: ValueError
    The exception is raised if both the `params` and one of the
    individual model parameters are passed at the same time.

)";

static PKArgs args___init__(0, 1, 8, false, false,
                                 {"params", "eta", "lambda1",
                                 "lambda2", "nepochs",
                                 "double_precision",
                                 "negative_class",
                                 "model_type"},
                                 "__init__", doc___init__);



void LinearModel::m__init__(const PKArgs& args) {
  m__dealloc__();
  double_precision = dt::LinearModelParams().double_precision;

  const Arg& arg_params           = args[0];
  const Arg& arg_eta              = args[1];
  const Arg& arg_lambda1          = args[2];
  const Arg& arg_lambda2          = args[3];
  const Arg& arg_nepochs          = args[4];
  const Arg& arg_double_precision = args[5];
  const Arg& arg_negative_class   = args[6];
  const Arg& arg_model_type       = args[7];


  bool defined_params           = !arg_params.is_none_or_undefined();
  bool defined_eta              = !arg_eta.is_none_or_undefined();
  bool defined_lambda1          = !arg_lambda1.is_none_or_undefined();
  bool defined_lambda2          = !arg_lambda2.is_none_or_undefined();
  bool defined_nepochs          = !arg_nepochs.is_none_or_undefined();
  bool defined_double_precision = !arg_double_precision.is_none_or_undefined();
  bool defined_negative_class   = !arg_negative_class.is_none_or_undefined();
  bool defined_model_type       = !arg_model_type.is_none_or_undefined();
  bool defined_individual_param = defined_eta ||
                                  defined_lambda1 || defined_lambda2 ||
                                  defined_nepochs || defined_double_precision ||
                                  defined_negative_class;

  init_py_params();

  if (defined_params) {
    if (defined_individual_param) {
      throw ValueError() << "You can either pass all the parameters with "
        << "`params` or any of the individual parameters with `eta`, "
        << "`lambda1`, `lambda2`, `nepochs`, "
        << "`double_precision`, `negative_class` or `model_type` "
        << "to `LinearModel` constructor, but not both at the same time";
    }

    py::otuple py_params_in = arg_params.to_otuple();
    py::oobj py_double_precision = py_params_in.get_attr("double_precision");
    double_precision = py_double_precision.to_bool_strict();

    init_dt_linearmodel();
    set_params_namedtuple(py_params_in);

  } else {
    if (defined_double_precision) {
      double_precision = arg_double_precision.to_bool_strict();
    }

    init_dt_linearmodel();
    if (defined_eta) set_eta(arg_eta);
    if (defined_lambda1) set_lambda1(arg_lambda1);
    if (defined_lambda2) set_lambda2(arg_lambda2);
    if (defined_nepochs) set_nepochs(arg_nepochs);
    if (defined_double_precision) set_double_precision(arg_double_precision);
    if (defined_negative_class) set_negative_class(arg_negative_class);
    if (defined_model_type) set_model_type(arg_model_type);
  }
}


void LinearModel::init_dt_linearmodel() {
  delete lm;
  if (double_precision) {
    lm = new dt::LinearModel<double>();
  } else {
    lm = new dt::LinearModel<float>();
  }
}


/**
 *  Deallocate memory.
 */
void LinearModel::m__dealloc__() {
  delete lm;
  delete py_params;
  delete colnames;
  lm = nullptr;
  py_params = nullptr;
  colnames = nullptr;
}


/**
 *  .fit(...)
 *  Do dataset validation and a call to `lm->dispatch_fit(...)` method.
 */

static const char* doc_fit =
R"(fit(self, X_train, y_train, X_validation=None, y_validation=None,
    nepochs_validation=1, validation_error=0.01,
    validation_average_niterations=1)
--

Train linear model on a dataset.

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
- :meth:`.predict` -- predict on a dataset.
- :meth:`.reset` -- reset the model.

)";

static PKArgs args_fit(2, 5, 0, false, false, {"X_train", "y_train",
                       "X_validation", "y_validation",
                       "nepochs_validation", "validation_error",
                       "validation_average_niterations"}, "fit",
                       doc_fit);


oobj LinearModel::fit(const PKArgs& args) {
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

  if (!lm->is_model_trained()) {
    delete colnames;
    colnames = new strvec(dt_X_train->get_names());
  }

  if (lm->is_model_trained() && dt_X_train->get_names() != *colnames) {
    throw ValueError() << "Training frame names cannot change for a trained "
                       << "model";
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
        lm->get_nepochs(),
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

  dt::LinearModelFitOutput output = lm->dispatch_fit(dt_X_train, dt_y,
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
 *  Perform dataset validation, make a call to `lm->predict(...)`,
 *  return frame with predictions.
 */

static const char* doc_predict =
R"(predict(self, X)
--

Make predictions for a dataset.

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
- :meth:`.fit` -- train model on a dataset.
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

  if (!lm->is_model_trained()) {
    throw ValueError() << "Cannot make any predictions, the model "
                          "should be trained first";
  }

  size_t ncols = lm->get_ncols();
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

  DataTable* dt_p = lm->predict(dt_X).release();
  py::oobj df_p = py::Frame::oframe(dt_p);

  return df_p;
}


/**
 *  .reset()
 *  Reset the model by making a call to `lm->reset()`.
 */

static const char* doc_reset =
R"(reset(self)
--

Reset linear model by resetting all the model weights, labels and
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


void LinearModel::reset(const PKArgs&) {
  lm->reset();
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


oobj LinearModel::get_labels() const {
  return lm->get_labels();
}


/**
 *  .model
 */

static const char* doc_model =
R"(
Trained models weights.

Parameters
----------
return: Frame
    A frame of shape `(nfeatures + 1, nlabels)`, where `nlabels` is
    the total number of labels the model was trained on, and
    :attr:`nfeatures <datatable.models.LinearModel.nfeatures>` is
    the total number of features.
)";

static GSArgs args_model("model", doc_model);

oobj LinearModel::get_model() const {
  if (!lm->is_model_trained()) return py::None();
  return lm->get_model();
}


void LinearModel::set_model(robj model) {
  DataTable* dt_model = model.to_datatable();
  if (dt_model == nullptr) return;

  if (dt_model->nrows() != (lm->get_nfeatures() + 1)) {
    throw ValueError() << "The number of rows in the model must be equal to the "
      << "number of features plus one, instead got: `" << dt_model->nrows()
      << "` and `" << lm->get_nfeatures() + 1 << "`, respectively";
  }

  size_t ncols = lm->get_model_type_trained() == dt::LinearModelType::BINOMIAL;
  if ((dt_model->ncols() + ncols) != lm->get_nlabels()) {
    throw ValueError() << "The number of columns in the model must be consistent "
      << "with the number of labels, instead got: `" << dt_model->ncols()
      << "` and `" << lm->get_nlabels() << "`, respectively";
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

  }
  lm->set_model(*dt_model);
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


oobj LinearModel::get_fi() const {
  return get_normalized_fi(true);
}

oobj LinearModel::get_normalized_fi(bool normalize) const {
  if (!lm->is_model_trained()) return py::None();
  return lm->get_fi(normalize);
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

)";

static GSArgs args_colnames(
  "colnames",
  doc_colnames
);


oobj LinearModel::get_colnames() const {
  if (lm->is_model_trained()) {
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


void LinearModel::set_colnames(robj py_colnames) {
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
 *  .eta
 */

static const char* doc_eta =
R"(
Step size, aka learning rate.

Parameters
----------
return: float
    Current `eta` value.

new_eta: float
    New `eta` value, should be positive.

except: ValueError
    The exception is raised when `new_eta` is not positive.
)";

static GSArgs args_eta(
  "eta",
  doc_eta
);


oobj LinearModel::get_eta() const {
  return py_params->get_attr("eta");
}


void LinearModel::set_eta(const Arg& py_eta) {
  double eta = py_eta.to_double();
  py::Validator::check_finite(eta, py_eta);
  py::Validator::check_positive(eta, py_eta);
  lm->set_eta(eta);
  py_params->replace(0, py_eta.robj());
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
  return py_params->get_attr("lambda1");
}


void LinearModel::set_lambda1(const Arg& py_lambda1) {
  double lambda1 = py_lambda1.to_double();
  py::Validator::check_finite(lambda1, py_lambda1);
  py::Validator::check_not_negative(lambda1, py_lambda1);
  lm->set_lambda1(lambda1);
  py_params->replace(1, py_lambda1.to_robj());
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
  return py_params->get_attr("lambda2");
}


void LinearModel::set_lambda2(const Arg& py_lambda2) {
  double lambda2 = py_lambda2.to_double();
  py::Validator::check_finite(lambda2, py_lambda2);
  py::Validator::check_not_negative(lambda2, py_lambda2);
  lm->set_lambda2(lambda2);
  py_params->replace(2, py_lambda2.to_robj());
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
  return py_params->get_attr("nepochs");
}


void LinearModel::set_nepochs(const Arg& arg_nepochs) {
  double nepochs = arg_nepochs.to_double();
  py::Validator::check_finite(nepochs, arg_nepochs);
  py::Validator::check_not_negative(nepochs, arg_nepochs);
  lm->set_nepochs(nepochs);
  py_params->replace(3, arg_nepochs.to_robj());
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
  return py_params->get_attr("double_precision");
}

void LinearModel::set_double_precision(const Arg& arg_double_precision) {
  if (lm->is_model_trained()) {
    throw ValueError() << "Cannot change " << arg_double_precision.name()
                       << "for a trained model, "
                       << "reset this model or create a new one";
  }
  double_precision = arg_double_precision.to_bool_strict();
  py_params->replace(4, arg_double_precision.to_robj());
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


oobj LinearModel::get_negative_class() const {
  return py_params->get_attr("negative_class");
}


void LinearModel::set_negative_class(const Arg& arg_negative_class) {
  if (lm->is_model_trained()) {
    throw ValueError() << "Cannot change " << arg_negative_class.name()
                       << " for a trained model, "
                       << "reset this model or create a new one";
  }
  bool negative_class = arg_negative_class.to_bool_strict();
  lm->set_negative_class(negative_class);
  py_params->replace(5, arg_negative_class.to_robj());
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
    The exception is raised when

    - trying to change this option for a model that has already been trained;
    - `new_model_type` value is not one of the following: `"binomial"`,
      `"multinomial"`, `"regression"` or `"auto"`.

See also
--------
- :attr:`.model_type_trained` -- the model type `LinearModel` has build.
)";

static GSArgs args_model_type(
  "model_type",
  doc_model_type
);


oobj LinearModel::get_model_type() const {
  return py_params->get_attr("model_type");
}


void LinearModel::set_model_type(const Arg& py_model_type) {
  if (lm->is_model_trained()) {
    throw ValueError() << "Cannot change `model_type` for a trained model, "
                       << "reset this model or create a new one";
  }
  std::string model_type = py_model_type.to_string();
  auto it = py::LinearModelNameType.find(model_type);
  if (it == py::LinearModelNameType.end() || it->second == dt::LinearModelType::NONE) {
    throw ValueError() << "Model type `" << model_type << "` is not supported";
  }

  lm->set_model_type(it->second);
  py_params->replace(6, py_model_type.to_robj());
}


/**
 *  .model_type_trained
 */

static const char* doc_model_type_trained =
R"(
The model type `LinearModel` has built.

Parameters
----------
return: str
    Could be one of the following: `"regression"`, `"binomial"`,
    `"multinomial"` or `"none"` for untrained model.

See also
--------
- :attr:`.model_type` -- the model type `LinearModel` should build.
)";

static GSArgs args_model_type_trained(
  "model_type_trained",
  doc_model_type_trained
);


oobj LinearModel::get_model_type_trained() const {
  dt::LinearModelType dt_model_type = lm->get_model_type_trained();
  std::string model_type = LinearModelTypeName.at(dt_model_type);
  return py::ostring(std::move(model_type));
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
  return *py_params;
}


void LinearModel::set_params_namedtuple(robj params_in) {
  py::otuple params_tuple = params_in.to_otuple();
  size_t n_params = params_tuple.size();

  if (n_params != 7) {
    throw ValueError() << "Tuple of LinearModel parameters should have 7 elements, "
                       << "got: " << n_params;
  }
  py::oobj py_eta = params_in.get_attr("eta");
  py::oobj py_lambda1 = params_in.get_attr("lambda1");
  py::oobj py_lambda2 = params_in.get_attr("lambda2");
  py::oobj py_nepochs = params_in.get_attr("nepochs");
  py::oobj py_double_precision = params_in.get_attr("double_precision");
  py::oobj py_negative_class = params_in.get_attr("negative_class");
  py::oobj py_model_type = params_in.get_attr("model_type");


  set_eta({py_eta, "`LinearModelParams.eta`"});
  set_lambda1({py_lambda1, "`LinearModelParams.lambda1`"});
  set_lambda2({py_lambda2, "`LinearModelParams.lambda2`"});
  set_nepochs({py_nepochs, "`LinearModelParams.nepochs`"});
  set_double_precision({py_double_precision, "`LinearModelParams.double_precision`"});
  set_negative_class({py_negative_class, "`LinearModelParams.negative_class`"});
  set_model_type({py_model_type, "`LinearModelParams.model_type`"});
}


oobj LinearModel::get_params_tuple() const {
  return otuple {get_eta(),
                 get_lambda1(),
                 get_lambda2(),
                 get_nepochs(),
                 get_double_precision(),
                 get_negative_class(),
                 get_model_type()
                };
}


void LinearModel::set_params_tuple(robj params) {
  py::otuple params_tuple = params.to_otuple();
  size_t n_params = params_tuple.size();
  if (n_params != 7) {
    throw ValueError() << "Tuple of `LinearModel` parameters should have 7 elements, "
                       << "got: " << n_params;
  }
  set_eta({params_tuple[0], "eta"});
  set_lambda1({params_tuple[1], "lambda1"});
  set_lambda2({params_tuple[2], "lambda2"});
  set_nepochs({params_tuple[3], "nepochs"});
  set_double_precision({params_tuple[4], "double_precision"});
  set_negative_class({params_tuple[5], "negative_class"});
  set_model_type({params_tuple[6], "model_type"});
}


void LinearModel::init_py_params() {
  static onamedtupletype py_params_ntt(
    "LinearModelParams",
    args_params.doc, {
      {args_eta.name,            args_eta.doc},
      {args_lambda1.name,          args_lambda1.doc},
      {args_lambda2.name,          args_lambda2.doc},
      {args_nepochs.name,          args_nepochs.doc},
      {args_double_precision.name, args_double_precision.doc},
      {args_negative_class.name,   args_negative_class.doc},
      {args_model_type.name,       args_model_type.doc}
    }
  );

  dt::LinearModelParams params;
  delete py_params;
  py_params = new py::onamedtuple(py_params_ntt);

  py_params->replace(0, py::ofloat(params.eta));
  py_params->replace(1, py::ofloat(params.lambda1));
  py_params->replace(2, py::ofloat(params.lambda2));
  py_params->replace(3, py::ofloat(params.nepochs));
  py_params->replace(4, py::obool(params.double_precision));
  py_params->replace(5, py::obool(params.negative_class));
  py_params->replace(6, py::ostring("auto"));
}



/**
 *  Pickling support.
 */
static PKArgs args___getstate__(
    0, 0, 0, false, false, {}, "__getstate__", nullptr);


oobj LinearModel::m__getstate__(const PKArgs&) {
  py::oobj py_api_version = py::oint(API_VERSION);
  py::oobj py_fi = get_normalized_fi(false);
  py::oobj py_labels = get_labels();
  py::oobj py_colnames = get_colnames();
  py::oobj py_params_tuple = get_params_tuple();
  py::oobj py_model = get_model();
  py::oobj py_model_type = get_model_type_trained();

  return otuple {py_api_version,
                 py_params_tuple,
                 py_fi,
                 py_labels,
                 py_colnames,
                 py_model_type,
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
  py::otuple py_params_tuple = pickle[1].to_otuple();

  double_precision = py_params_tuple[5].to_bool_strict();
  init_dt_linearmodel();
  init_py_params();
  set_params_tuple(pickle[1]);
  if (pickle[2].is_frame()) {
    lm->set_fi(*pickle[2].to_datatable());
  }

  if (pickle[3].is_frame()) {
    lm->set_labels(*pickle[3].to_datatable());
  }
  set_colnames(pickle[4]);

  auto model_type = py::LinearModelNameType.at(pickle[5].to_string());
  lm->set_model_type_trained(model_type);
  set_model(pickle[6]);
}




//------------------------------------------------------------------------------
// py::LinearModel::Type
//------------------------------------------------------------------------------

static const char* doc_LinearModel =
R"(Regularized linear model with stochastic gradient descent learning.

)";

void LinearModel::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.models.LinearModel");
  xt.set_class_doc(doc_LinearModel);

  xt.add(CONSTRUCTOR(&LinearModel::m__init__, args___init__));
  xt.add(DESTRUCTOR(&LinearModel::m__dealloc__));

  // Input parameters
  xt.add(GETTER(&LinearModel::get_params_namedtuple, args_params));
  xt.add(GETSET(&LinearModel::get_eta, &LinearModel::set_eta, args_eta));
  xt.add(GETSET(&LinearModel::get_lambda1, &LinearModel::set_lambda1, args_lambda1));
  xt.add(GETSET(&LinearModel::get_lambda2, &LinearModel::set_lambda2, args_lambda2));
  xt.add(GETSET(&LinearModel::get_nepochs, &LinearModel::set_nepochs, args_nepochs));
  xt.add(GETTER(&LinearModel::get_double_precision, args_double_precision));
  xt.add(GETSET(&LinearModel::get_negative_class, &LinearModel::set_negative_class, args_negative_class));
  xt.add(GETSET(&LinearModel::get_model_type, &LinearModel::set_model_type, args_model_type));

  // Model and features
  xt.add(GETTER(&LinearModel::get_labels, args_labels));
  xt.add(GETTER(&LinearModel::get_model_type_trained, args_model_type_trained));
  xt.add(GETTER(&LinearModel::get_model, args_model));
  xt.add(GETTER(&LinearModel::get_fi, args_fi));
  xt.add(GETTER(&LinearModel::get_colnames, args_colnames));

  // Fit, predict and reset
  xt.add(METHOD(&LinearModel::fit, args_fit));
  xt.add(METHOD(&LinearModel::predict, args_predict));
  xt.add(METHOD(&LinearModel::reset, args_reset));

  // Pickling and unpickling
  xt.add(METHOD(&LinearModel::m__getstate__, args___getstate__));
  xt.add(METHOD(&LinearModel::m__setstate__, args___setstate__));
}




} // namespace py
