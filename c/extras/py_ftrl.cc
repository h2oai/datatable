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
#include "frame/py_frame.h"
#include "python/float.h"
#include "python/int.h"
#include "python/tuple.h"
#include "python/string.h"
#include "str/py_str.h"
#include "extras/py_ftrl.h"
#include <extras/py_validator.h>

namespace py {


PKArgs Ftrl::Type::args___init__(0, 2, 7, false, false,
                                 {"params", "labels", "alpha", "beta", "lambda1",
                                 "lambda2", "nbins", "nepochs", "interactions"},
                                 "__init__", nullptr);

NoArgs Ftrl::Type::fn___getstate__("__getstate__", nullptr);
PKArgs Ftrl::Type::fn___setstate__(1, 0, 0, false, false, {"state"},
                                   "__setstate__", nullptr);

static const char* doc_alpha        = "`alpha` in per-coordinate FTRL-Proximal algorithm";
static const char* doc_beta         = "`beta` in per-coordinate FTRL-Proximal algorithm";
static const char* doc_lambda1      = "L1 regularization parameter";
static const char* doc_lambda2      = "L2 regularization parameter";
static const char* doc_nbins        = "Number of bins to be used for the hashing trick";
static const char* doc_nepochs      = "Number of epochs to train a model";
static const char* doc_interactions = "Switch to enable second order feature interactions";

static onamedtupletype& _get_params_namedtupletype() {
  static onamedtupletype ntt(
    "FtrlParams",
    "FTRL model parameters",
    {{"alpha",        doc_alpha},
     {"beta",         doc_beta},
     {"lambda1",      doc_lambda1},
     {"lambda2",      doc_lambda2},
     {"nbins",        doc_nbins},
     {"nepochs",      doc_nepochs},
     {"interactions", doc_interactions}}
  );
  return ntt;
}


void Ftrl::m__init__(PKArgs& args) {
  dtft = nullptr;
  dt::FtrlParams dt_params = dt::Ftrl::default_params;

  bool defined_params   = !args[0].is_none_or_undefined();
  bool defined_labels   = !args[1].is_none_or_undefined();
  bool defined_alpha    = !args[2].is_none_or_undefined();
  bool defined_beta     = !args[3].is_none_or_undefined();
  bool defined_lambda1  = !args[4].is_none_or_undefined();
  bool defined_lambda2  = !args[5].is_none_or_undefined();
  bool defined_nbins    = !args[6].is_none_or_undefined();
  bool defined_nepochs  = !args[7].is_none_or_undefined();
  bool defined_interactions    = !args[8].is_none_or_undefined();

  if (defined_params) {
    if (defined_alpha || defined_beta || defined_lambda1 || defined_lambda2 ||
        defined_nbins || defined_nepochs || defined_interactions) {
      throw TypeError() << "You can either pass all the parameters with "
            << "`params` or any of the individual parameters with `alpha`, "
            << "`beta`, `lambda1`, `lambda2`, `nbins`, `nepochs` or "
            << "`interactions` to Ftrl constructor, but not both at the "
            << "same time";
    }
    py::otuple py_params = args[0].to_otuple();
    py::oobj py_alpha = py_params.get_attr("alpha");
    py::oobj py_beta = py_params.get_attr("beta");
    py::oobj py_lambda1 = py_params.get_attr("lambda1");
    py::oobj py_lambda2 = py_params.get_attr("lambda2");
    py::oobj py_nbins = py_params.get_attr("nbins");
    py::oobj py_nepochs = py_params.get_attr("nepochs");
    py::oobj py_interactions = py_params.get_attr("interactions");

    dt_params.alpha = py_alpha.to_double();
    dt_params.beta = py_beta.to_double();
    dt_params.lambda1 = py_lambda1.to_double();
    dt_params.lambda2 = py_lambda2.to_double();
    dt_params.nbins = static_cast<uint64_t>(py_nbins.to_size_t());
    dt_params.nepochs = py_nepochs.to_size_t();
    dt_params.interactions = py_interactions.to_bool_strict();

    py::Validator::check_positive<double>(dt_params.alpha, py_alpha);
    py::Validator::check_not_negative<double>(dt_params.beta, py_beta);
    py::Validator::check_not_negative<double>(dt_params.lambda1, py_lambda1);
    py::Validator::check_not_negative<double>(dt_params.lambda2, py_lambda2);
    py::Validator::check_not_negative<double>(dt_params.nbins, py_nbins);

  } else {

    if (defined_alpha) {
      dt_params.alpha = args[2].to_double();
      py::Validator::check_positive<double>(dt_params.alpha, args[2]);
    }

    if (defined_beta) {
      dt_params.beta = args[3].to_double();
      py::Validator::check_not_negative<double>(dt_params.beta, args[3]);
    }

    if (defined_lambda1) {
      dt_params.lambda1 = args[4].to_double();
      py::Validator::check_not_negative<double>(dt_params.lambda1, args[4]);
    }

    if (defined_lambda2) {
      dt_params.lambda2 = args[5].to_double();
      py::Validator::check_not_negative<double>(dt_params.lambda2, args[5]);
    }

    if (defined_nbins) {
      dt_params.nbins = static_cast<uint64_t>(args[6].to_size_t());
      py::Validator::check_positive<uint64_t>(dt_params.nbins, args[6]);
    }

    if (defined_nepochs) {
      dt_params.nepochs = args[7].to_size_t();
    }

    if (defined_interactions) {
      dt_params.interactions = args[8].to_bool_strict();
    }
  }

  if (defined_labels) {
    set_labels(args[1]);
  } else {
    py::olist py_list(0);
    set_labels(py::robj(py_list));
  }

  dtft = new std::vector<dtftptr>;
  init_dtft(dt_params);
  reg_type = RegType::NONE;
}


void Ftrl::init_dtft(dt::FtrlParams dt_params) {
  size_t nlabels = labels.size();
  xassert(nlabels > 0);
  // If there is only two labels provided, we only need
  // one classifier.
  size_t ndtft = nlabels - (nlabels == 2);
  dtft->clear();
  dtft->reserve(ndtft);
  for (size_t i = 0; i < ndtft; ++i) {
    dtft->push_back(dtftptr(new dt::Ftrl(dt_params)));
  }
}


void Ftrl::m__dealloc__() {
  PyObject* py_labels = std::move(labels).release();
  Py_XDECREF(py_labels);
  if (dtft != nullptr) {
    delete dtft;
    dtft = nullptr;
  }
  reset_feature_names();
}


const char* Ftrl::Type::classname() {
  return "datatable.models.Ftrl";
}


// TODO: use the above doc strings here.
const char* Ftrl::Type::classdoc() {
  return R"(Follow the Regularized Leader (FTRL) model with hashing trick.

See this reference for more details:
https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf

Parameters
----------
alpha : float
    `alpha` in per-coordinate learning rate formula.
beta : float
    `beta` in per-coordinate learning rate formula.
lambda1 : float
    L1 regularization parameter.
lambda2 : float
    L2 regularization parameter.
nbins : int
    Number of bins to be used after the hashing trick.
nepochs : int
    Number of epochs to train for.
interactions : bool
    Switch to enable second order feature interactions.
)";
}


void Ftrl::Type::init_methods_and_getsets(Methods& mm, GetSetters& gs) {
  mm.add<&Ftrl::m__getstate__, fn___getstate__>();
  mm.add<&Ftrl::m__setstate__, fn___setstate__>();

  gs.add<&Ftrl::get_labels, &Ftrl::set_labels>(
    "labels",
    R"(List of labels for multinomial regression.)"
  );

  gs.add<&Ftrl::get_model, &Ftrl::set_model>(
    "model",
    R"(Tuple of model frames. Each frame has two columns, i.e. `z` and `n`,
    and `nbins` rows, where `nbins` is a number of bins for the hashing trick.
    Both column types are `float64`.)"
  );

  gs.add<&Ftrl::get_fi>(
    "feature_importances",
    R"(One-column frame with the overall weight contributions calculated
    feature-wise during training and predicting. It can be interpreted as
    a feature importance information.)"
  );

  gs.add<&Ftrl::get_params_namedtuple, &Ftrl::set_params_namedtuple>(
    "params",
    "FTRL model parameters"
  );

  gs.add<&Ftrl::get_colname_hashes>(
    "colname_hashes",
    "Column name hashes"
  );

  gs.add<&Ftrl::get_alpha, &Ftrl::set_alpha>("alpha", doc_alpha);
  gs.add<&Ftrl::get_beta, &Ftrl::set_beta>("beta", doc_beta);
  gs.add<&Ftrl::get_lambda1, &Ftrl::set_lambda1>("lambda1", doc_lambda1);
  gs.add<&Ftrl::get_lambda2, &Ftrl::set_lambda2>("lambda2", doc_lambda2);
  gs.add<&Ftrl::get_nbins, &Ftrl::set_nbins>("nbins", doc_nbins);
  gs.add<&Ftrl::get_nepochs, &Ftrl::set_nepochs>("nepochs", doc_nepochs);
  gs.add<&Ftrl::get_interactions, &Ftrl::set_interactions>("interactions", doc_interactions);

  mm.add<&Ftrl::fit, args_fit>();
  mm.add<&Ftrl::predict, args_predict>();
  mm.add<&Ftrl::reset, args_reset>();
}


PKArgs Ftrl::Type::args_fit(2, 0, 0, false, false, {"X", "y"}, "fit",
R"(fit(self, X, y)
--

Train an FTRL model on a dataset.

Parameters
----------
X: Frame
    Frame of shape (nrows, ncols) to be trained on.

y: Frame
    Frame of shape (nrows, 1), i.e. the target column.
    This column must have a `bool` type.

Returns
-------
    None
)");


void Ftrl::fit(const PKArgs& args) {
  if (args[0].is_undefined()) {
    throw ValueError() << "Training frame parameter is missing";
  }

  if (args[1].is_undefined()) {
    throw ValueError() << "Target frame parameter is missing";
  }

  DataTable* dt_X = args[0].to_frame();
  DataTable* dt_y = args[1].to_frame();
  if (dt_X == nullptr || dt_y == nullptr) return;

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

  SType stype_y = dt_y->columns[0]->stype();
  switch (stype_y) {
    case SType::BOOL:    fit_binomial(dt_X, dt_y); break;
    case SType::INT8:    fit_regression<int8_t>(dt_X, dt_y); break;
    case SType::INT16:   fit_regression<int16_t>(dt_X, dt_y); break;
    case SType::INT32:   fit_regression<int32_t>(dt_X, dt_y); break;
    case SType::INT64:   fit_regression<int64_t>(dt_X, dt_y); break;
    case SType::FLOAT32: fit_regression<float>(dt_X, dt_y); break;
    case SType::FLOAT64: fit_regression<double>(dt_X, dt_y); break;
    case SType::STR32:   [[clang::fallthrough]];
    case SType::STR64:   fit_multinomial(dt_X, dt_y); break;
    default:             throw TypeError() << "Cannot predict for a column "
                                           << "of type `" << stype_y << "`";
  }

  size_t nfeatures = (*dtft)[0]->get_nfeatures();
  // Second condition means: `interactions` parameter was changed meanwhile
  if (feature_names == nullptr || feature_names->nrows != nfeatures) {
    reset_feature_names();
    size_t ncols = dt_X->ncols;
    const std::vector<std::string>& column_names = dt_X->get_names();

    dt::fixed_height_string_col scol(nfeatures);
    dt::fixed_height_string_col::buffer sb(scol);
    sb.commit_and_start_new_chunk(0);
    for (const auto& feature_name : column_names) {
      sb.write(feature_name);
    }

    if ((*dtft)[0]->get_params().interactions) {
      for (size_t i = 0; i < ncols - 1; ++i) {
        for (size_t j = i + 1; j < ncols; ++j) {
          std::string feature_name = column_names[i] + ":" + column_names[j];
          sb.write(feature_name);
        }
      }
    }

    sb.order();
    sb.commit_and_start_new_chunk(nfeatures);
    feature_names = new DataTable({std::move(scol).to_column()}, {"feature_name"});
  }

}


void Ftrl::fit_binomial(DataTable* dt_X, DataTable* dt_y) {
  if (reg_type != RegType::NONE && reg_type != RegType::BINOMIAL) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from binomial. To train it "
                         "in a binomial mode this model should be reset.";
  }
  reg_type = RegType::BINOMIAL;
  (*dtft)[0]->fit<int8_t>(dt_X, dt_y->columns[0], sigmoid);
}


template <typename T>
void Ftrl::fit_regression(DataTable* dt_X, DataTable* dt_y) {
  if (reg_type != RegType::NONE && reg_type != RegType::REGRESSION) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from regression. To train it "
                         "in a binomial mode this model should be reset.";
  }
  reg_type = RegType::REGRESSION;
  (*dtft)[0]->fit<T>(dt_X, dt_y->columns[0], identity);
}


void Ftrl::fit_multinomial(DataTable* dt_X, DataTable* dt_y) {
  if (reg_type != RegType::NONE && reg_type != RegType::MULTINOMIAL) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from multinomial. To train it "
                         "in a binomial mode this model should be reset.";
  }
  reg_type = RegType::MULTINOMIAL;

  // Due to `m__init__()` calling `init_dtft()`, number of classifiers
  // is consistent with the number of labels from the very beginning.
  // If number of labels changes afterwards with `set_labels()`,
  // we need to re-initialize classifiers.
  size_t nlabels = labels.size();
  if (nlabels != dtft->size()) {
    init_dtft((*dtft)[0]->get_params());
  }

  dtptr dt_ys;
  std::vector<BoolColumn*> c_y;
  c_y.reserve(dtft->size());
  if (nlabels > 1) {
    dt_ys = dtptr(dt::split_into_nhot(dt_y->columns[0], ','));
    const strvec& colnames = dt_ys->get_names();
    size_t nmissing = 0;

    for (size_t i = 0; i < nlabels; ++i) {
      BoolColumn* col;
      std::string label = labels[i].to_string();
      auto it = std::find(colnames.begin(), colnames.end(), label);

      if (it == colnames.end()) {
        nmissing++;
        Warning w = DatatableWarning();
        w << "Label '" << label << "' was not found in a target frame";
        col = static_cast<BoolColumn*>(
                Column::new_data_column(SType::BOOL, dt_y->nrows)
              );
        auto d_y = static_cast<bool*>(col->data_w());
        for (size_t j = 0; j < col->nrows; ++j) d_y[j] = false;
      } else {
        size_t pos = static_cast<size_t>(it - colnames.begin());
        col = static_cast<BoolColumn*>(dt_ys->columns[pos]);
      }
      c_y.push_back(col);
    }

    if (nlabels != dt_ys->ncols + nmissing) {
       // TODO: make this message more user friendly.
       throw ValueError() << "Target column contains unknown labels";
    }
  } else {
    throw ValueError() << "For multinomial regression a list of labels "
                          "should be provided";
  }

  // Train all the classifiers. NB: when there is two labels,
  // `init_dtft()` creates only one classifier, just like in a
  // binomial case.
  size_t ndtft = dtft->size();
  for (size_t i = 0; i < ndtft; ++i) {
    (*dtft)[i]->fit<int8_t>(dt_X, c_y[i], sigmoid);
  }
}


PKArgs Ftrl::Type::args_predict(1, 0, 0, false, false, {"X"}, "predict",
R"(predict(self, X)
--

Make predictions for a dataset.

Parameters
----------
X: Frame
    Frame of shape (nrows, ncols) to make predictions for.
    It must have the same number of columns as the training frame.

Returns
-------
    A new frame of shape (nrows, 1) with the predicted probability
    for each row of frame X.
)");


oobj Ftrl::predict(const PKArgs& args) {
  if (args[0].is_undefined()) {
    throw ValueError() << "Frame to make predictions for is missing";
  }

  DataTable* dt_X = args[0].to_frame();
  if (dt_X == nullptr) return Py_None;

  if (!(*dtft)[0]->is_trained()) {
    throw ValueError() << "Cannot make any predictions, train or set "
                       << "the model first";
  }

  size_t ncols = (*dtft)[0]->get_ncols();
  if (dt_X->ncols != ncols && ncols != 0) {
    throw ValueError() << "Can only predict on a frame that has " << ncols
                       << " column" << (ncols == 1? "" : "s")
                       << ", i.e. has the same number of features as "
                          "was used for model training";
  }

  size_t nlabels = labels.size();
  size_t ndtft = dtft->size();

  // If number of labels is different from the number of classifiers,
  // then there is something wrong. Unless we do a multinomial
  // regression with only two labels.
  if (nlabels != ndtft && !(nlabels == 2 && ndtft == 1)) {
    if (ndtft == 1) {
      // Binomial and regression cases
      throw ValueError() << "Cannot make any predictions with the labels "
                            "supplied, as the model was trained in "
                            "a binomial/regression mode";
    } else {
      // Multinomial case
      throw ValueError() << "Can only make predictions for " << dtft->size()
                        << " labels, i.e. the same number of labels as "
                        << "was used for model training";
    }
  }

  // Determine which link function we should use.
  double (*f)(double);
  switch (reg_type) {
    case RegType::REGRESSION  : f = identity; break;
    case RegType::BINOMIAL    : f = sigmoid; break;
    case RegType::MULTINOMIAL : (nlabels == 2)? f = sigmoid : f = std::exp; break;
    // If this error is thrown, it means that `fit()` and `reg_type`
    // went out of sync, so there is a bug in the code.
    default : throw ValueError() << "Cannot make any predictions, "
                                 << "the model was trained in an unknown mode";
  }

  // Make predictions and `cbind` targets.
  DataTable* dt_y = nullptr;
  std::vector<std::string> names(1);
  std::vector<DataTable*> dt(1);
  for (size_t i = 0; i < dtft->size(); ++i) {
    DataTable* dt_yi = (*dtft)[i]->predict(dt_X, f).release();
    names[0] = labels[i].to_string();
    dt_yi->set_names(names);
    dt[0] = dt_yi;
    if (i) dt_y->cbind(dt);
    else dt_y = dt_yi;
  }

  // For multinomial case, when there is more than two labels,
  // apply `softmax` function. NB: we already applied `std::exp`
  // function to predictions, so only need to normalize probabilities here.
  if (nlabels > 2) normalize_rows(dt_y);

  py::oobj df_y = py::oobj::from_new_reference(
                         py::Frame::from_datatable(dt_y)
                  );

  return df_y;
}


/*
*  Sigmoid function.
*/
inline double Ftrl::sigmoid(double x) {
  return 1.0 / (1.0 + std::exp(-x));
}


/*
*  Identity function.
*/
inline double Ftrl::identity(double x) {
  return x;
}


void Ftrl::normalize_rows(DataTable* dt) {
  size_t nrows = dt->nrows;
  size_t ncols = dt->ncols;

  std::vector<double*> d_cs;
  d_cs.reserve(ncols);
  for (size_t j = 0; j < ncols; ++j) {
    double* d_c = static_cast<double*>(dt->columns[j]->data_w());
    d_cs.push_back(d_c);
  }

  #pragma omp parallel for num_threads(config::nthreads)
  for (size_t i = 0; i < nrows; ++i) {
    double denom = 0.0;
    for (size_t j = 0; j < ncols; ++j) {
      denom += d_cs[j][i];
    }
    for (size_t j = 0; j < ncols; ++j) {
      d_cs[j][i] = d_cs[j][i] / denom;
    }
  }
}


void Ftrl::reset_feature_names() {
  if (feature_names != nullptr) {
    delete feature_names;
    feature_names = nullptr;
  }
}

NoArgs Ftrl::Type::args_reset("reset",
R"(reset(self)
--

Reset FTRL model and feature importance information,
i.e. initialize model and importance frames with zeros.

Parameters
----------
    None

Returns
-------
    None
)");


void Ftrl::reset(const NoArgs&) {
  reg_type = RegType::NONE;
  for (size_t i = 0; i < dtft->size(); ++i) {
    (*dtft)[i]->reset_model();
    (*dtft)[i]->reset_fi();
  }
  reset_feature_names();
}


/*
*  Getters and setters.
*/
oobj Ftrl::get_labels() const {
  return py::olist(labels);
}


oobj Ftrl::get_model() const {
  if ((*dtft)[0]->is_trained()) {
    size_t ndtft = dtft->size();
    py::otuple models(ndtft);

    for (size_t i = 0; i < ndtft; ++i) {
      DataTable* dt_model = (*dtft)[i]->get_model();
      py::oobj df_model = py::oobj::from_new_reference(
                            py::Frame::from_datatable(dt_model)
                          );
      models.set(i, df_model);
    }
    return std::move(models);
  } else {
    return py::None();
  }
}


oobj Ftrl::get_fi() const {
  // If model was trained, return feature importance info.
  if ((*dtft)[0]->is_trained()) {
    size_t ndtft = dtft->size();
    DataTable* dt_feature_importances;
    DataTable* dt_fi;
    // If there is more than one classifier, average feature importance row-wise.
    if (ndtft > 1) {
      // Create datatable for averaged feature importance info.
      size_t fi_nrows = (*dtft)[0]->get_fi()->nrows;
      Column* col_fi = Column::new_data_column(SType::FLOAT64, fi_nrows);
      dt_fi = new DataTable({col_fi}, {"feature_importance"});
      auto d_fi = static_cast<double*>(dt_fi->columns[0]->data_w());

      std::vector<double*> d_fis;
      d_fis.reserve(ndtft);
      for (size_t j = 0; j < ndtft; ++j) {
        DataTable* dt_fi_j = (*dtft)[j]->get_fi();
        double* d_fi_j = static_cast<double*>(dt_fi_j->columns[0]->data_w());
        d_fis.push_back(d_fi_j);
      }

      for (size_t i = 0; i < fi_nrows; ++i) {
        d_fi[i] = 0.0;
        for (size_t j = 0; j < ndtft; ++j) d_fi[i] += d_fis[j][i];
        d_fi[i] /= ndtft;
      }
    } else {
    	// If there is just one classifier, simply return `fi`.
      dt_fi = (*dtft)[0]->get_fi();
    }
    normalize_fi(static_cast<RealColumn<double>*>(dt_fi->columns[0]));
    // TODO: memoize `dt_feature_importances`
    dt_feature_importances = feature_names->copy();
    std::vector<DataTable*> dt(1);
    dt[0] = dt_fi;
    dt_feature_importances->cbind(dt);
    py::oobj df_fi = py::oobj::from_new_reference(
                       py::Frame::from_datatable(dt_feature_importances)
                     );
    return df_fi;
  } else {
  	// If model was not trained, return `None`.
    return py::None();
  }
}



/*
* Normalize a column of feature importances to [0; 1]
* This column has only positive values, so we simply divide its
* content by `col->max()`. Another option is to do min-max normalization,
* but this may lead to some features having zero importance,
* while in reality they don't.
*/
void Ftrl::normalize_fi(RealColumn<double>* col) {
  double max = col->max();
  double epsilon = std::numeric_limits<double>::epsilon();
  double* data = col->elements_w();

  double norm_factor = 1.0;
  if (fabs(max) > epsilon) norm_factor = 1 / max;

  for (size_t i = 0; i < col->nrows; ++i) {
    data[i] *= norm_factor;
  }
  col->get_stats()->reset();
}


oobj Ftrl::get_fi_tuple() const {
  if ((*dtft)[0]->is_trained()) {
    size_t ndtft = dtft->size();
    py::otuple fi(ndtft);
    for (size_t i = 0; i < ndtft; ++i) {
      DataTable* dt_fi = (*dtft)[i]->get_fi();
      py::oobj df_fi = py::oobj::from_new_reference(
                            py::Frame::from_datatable(dt_fi)
                       );
      fi.set(i, df_fi);
    }
    return std::move(fi);
  } else {
    return py::None();
  }
}


oobj Ftrl::get_params_namedtuple() const {
  py::onamedtuple params(_get_params_namedtupletype());
  params.set(0, get_alpha());
  params.set(1, get_beta());
  params.set(2, get_lambda1());
  params.set(3, get_lambda2());
  params.set(4, get_nbins());
  params.set(5, get_nepochs());
  params.set(6, get_interactions());
  return std::move(params);
}


oobj Ftrl::get_params_tuple() const {
  py::otuple params(7);
  params.set(0, get_alpha());
  params.set(1, get_beta());
  params.set(2, get_lambda1());
  params.set(3, get_lambda2());
  params.set(4, get_nbins());
  params.set(5, get_nepochs());
  params.set(6, get_interactions());
  return std::move(params);
}


oobj Ftrl::get_alpha() const {
  return py::ofloat((*dtft)[0]->get_alpha());
}


oobj Ftrl::get_beta() const {
  return py::ofloat((*dtft)[0]->get_beta());
}


oobj Ftrl::get_lambda1() const {
  return py::ofloat((*dtft)[0]->get_lambda1());
}


oobj Ftrl::get_lambda2() const {
  return py::ofloat((*dtft)[0]->get_lambda2());
}


oobj Ftrl::get_nbins() const {
  return py::oint(static_cast<size_t>((*dtft)[0]->get_nbins()));
}


oobj Ftrl::get_nepochs() const {
  return py::oint((*dtft)[0]->get_nepochs());
}


oobj Ftrl::get_interactions() const {
  return (*dtft)[0]->get_interactions()? True() : False();
}


oobj Ftrl::get_colname_hashes() const {
  if ((*dtft)[0]->is_trained()) {
    size_t ncols = (*dtft)[0]->get_ncols();
    py::otuple py_colname_hashes(ncols);
    std::vector<uint64_t> colname_hashes = (*dtft)[0]->get_colnames_hashes();
    for (size_t i = 0; i < ncols; ++i) {
      size_t h = static_cast<size_t>(colname_hashes[i]);
      py_colname_hashes.set(i, py::oint(h));
    }
    return std::move(py_colname_hashes);
  } else {
    return py::None();
  }
}


void Ftrl::set_labels(robj py_labels) {
  py::olist labels_in = py_labels.to_pylist();
  size_t nlabels = labels_in.size();
  if (nlabels == 1) {
    throw ValueError() << "List of labels can not have one element";
  }
  // This ensures that we always have at least one classifier
  if (nlabels == 0) {
    labels_in.append(py::ostring("target"));
  }
  labels = labels_in;
}


void Ftrl::set_model(robj model) {
  size_t ndtft = dtft->size();
  // Reset model if it was assigned `None` in Python
  if (model.is_none()) {
    reg_type = RegType::NONE;
    if ((*dtft)[0]->is_trained()) {
      for (size_t i = 0; i < ndtft; ++i) {
        (*dtft)[i]->reset_model();
      }
    }
    return;
  }

  size_t nlabels = labels.size();
  py::otuple py_model = model.to_otuple();
  if (py_model.size() != nlabels) {
    throw ValueError() << "Number of models should be the same as number "
                       << "of labels, i.e. " << nlabels << ", got "
                       << py_model.size();

  }

  if (nlabels > 1) {
    reg_type = RegType::MULTINOMIAL;
  } else {
    // This could either be `RegType::REGRESSION` or `RegType::BINOMIAL`,
    // however, there is no way to check it by just looking at the model.
    // May need to disable model setter or have an explicit getter/setter
    // for `reg_type`. For the moment we default this case to
    // `RegType::BINOMIAL`.
    reg_type = RegType::BINOMIAL;
  }

  // Initialize classifiers
  init_dtft((*dtft)[0]->get_params());

  for (size_t i = 0; i < nlabels; ++i) {
    DataTable* dt_model_in = py_model[i].to_frame();
    const std::vector<std::string>& model_cols_in = dt_model_in->get_names();

    if (dt_model_in->nrows != (*dtft)[0]->get_nbins() || dt_model_in->ncols != 2) {
      throw ValueError() << "Element " << i << ": "
                         << "FTRL model frame must have " << (*dtft)[0]->get_nbins()
                         << " rows, and 2 columns, whereas your frame has "
                         << dt_model_in->nrows << " rows and "
                         << dt_model_in->ncols << " column"
                         << (dt_model_in->ncols == 1? "": "s");

    }

    if (model_cols_in != dt::Ftrl::model_colnames) {
      throw ValueError() << "Element " << i << ": "
                         << "FTRL model frame must have columns named `z` and "
                         << "`n`, whereas your frame has the following column "
                         << "names: `" << model_cols_in[0]
                         << "` and `" << model_cols_in[1] << "`";
    }

    if (dt_model_in->columns[0]->stype() != SType::FLOAT64 ||
      dt_model_in->columns[1]->stype() != SType::FLOAT64) {
      throw ValueError() << "Element " << i << ": "
                         << "FTRL model frame must have both column types as "
                         << "`float64`, whereas your frame has the following "
                         << "column types: `"
                         << dt_model_in->columns[0]->stype()
                         << "` and `" << dt_model_in->columns[1]->stype() << "`";
    }

    if (has_negative_n(dt_model_in)) {
      throw ValueError() << "Element " << i << ": "
                         << "Values in column `n` cannot be negative";
    }

    (*dtft)[i]->set_model(dt_model_in);
  }
}


void Ftrl::set_params_namedtuple(robj params) {
  set_alpha(params.get_attr("alpha"));
  set_beta(params.get_attr("beta"));
  set_lambda1(params.get_attr("lambda1"));
  set_lambda2(params.get_attr("lambda2"));
  set_nbins(params.get_attr("nbins"));
  set_nepochs(params.get_attr("nepochs"));
  set_interactions(params.get_attr("interactions"));
  // TODO: check that there are no unknown parameters
}


void Ftrl::set_params_tuple(robj params) {
  py::otuple params_tuple = params.to_otuple();
  size_t n_params = params_tuple.size();
  if (n_params != 7) {
    throw ValueError() << "Tuple of FTRL parameters should have 7 elements, "
                       << "got: " << n_params;
  }
  set_alpha(params_tuple[0]);
  set_beta(params_tuple[1]);
  set_lambda1(params_tuple[2]);
  set_lambda2(params_tuple[3]);
  set_nbins(params_tuple[4]);
  set_nepochs(params_tuple[5]);
  set_interactions(params_tuple[6]);
}


void Ftrl::set_alpha(robj py_alpha) {
  double alpha = py_alpha.to_double();
  py::Validator::check_positive(alpha, py_alpha);
  for (size_t i = 0; i < dtft->size(); ++i) {
    (*dtft)[i]->set_alpha(alpha);
  }
}


void Ftrl::set_beta(robj py_beta) {
  double beta = py_beta.to_double();
  py::Validator::check_not_negative(beta, py_beta);
  for (size_t i = 0; i < dtft->size(); ++i) {
    (*dtft)[i]->set_beta(beta);
  }
}


void Ftrl::set_lambda1(robj py_lambda1) {
  double lambda1 = py_lambda1.to_double();
  py::Validator::check_not_negative(lambda1, py_lambda1);
  for (size_t i = 0; i < dtft->size(); ++i) {
    (*dtft)[i]->set_lambda1(lambda1);
  }
}


void Ftrl::set_lambda2(robj py_lambda2) {
  double lambda2 = py_lambda2.to_double();
  py::Validator::check_not_negative(lambda2, py_lambda2);
  for (size_t i = 0; i < dtft->size(); ++i) {
    (*dtft)[i]->set_lambda2(lambda2);
  }
}


void Ftrl::set_nbins(robj py_nbins) {
  if ((*dtft)[0]->is_trained()) {
    throw ValueError() << "Cannot set `nbins` for a trained model, "
                       << "reset this model or create a new one";
  }

  uint64_t nbins = static_cast<uint64_t>(py_nbins.to_size_t());
  py::Validator::check_positive(nbins, py_nbins);
  for (size_t i = 0; i < dtft->size(); ++i) {
    (*dtft)[i]->set_nbins(nbins);
  }
}


void Ftrl::set_nepochs(robj py_nepochs) {
  size_t nepochs = py_nepochs.to_size_t();
  for (size_t i = 0; i < dtft->size(); ++i) {
    (*dtft)[i]->set_nepochs(nepochs);
  }
}


void Ftrl::set_interactions(robj py_interactions) {
  bool interactions = py_interactions.to_bool_strict();
  for (size_t i = 0; i < dtft->size(); ++i) {
    (*dtft)[i]->set_interactions(interactions);
  }
}


/*
*  Model validation methods.
*/
bool Ftrl::has_negative_n(DataTable* dt) const {
  auto c_n = static_cast<RealColumn<double>*>(dt->columns[1]);
  auto d_n = c_n->elements_r();
  for (size_t i = 0; i < dt->nrows; ++i) {
    if (d_n[i] < 0) return true;
  }
  return false;
}


/*
*  Pickling / unpickling methods.
*/
oobj Ftrl::m__getstate__(const NoArgs&) {
  py::otuple pickle(6);
  py::oobj params = get_params_tuple();
  py::oobj model = get_model();
  py::oobj fi = get_fi_tuple();
  py::oobj py_reg_type = py::oint(static_cast<int32_t>(reg_type));
  py::oobj df_feature_names;
  if (feature_names == nullptr) {
    df_feature_names = py::None();
  } else {
    df_feature_names = py::oobj::from_new_reference(
                         py::Frame::from_datatable(feature_names->copy())
                       );
  }

  pickle.set(0, params);
  pickle.set(1, model);
  pickle.set(2, fi);
  pickle.set(3, df_feature_names);
  pickle.set(4, labels);
  pickle.set(5, py_reg_type);
  return std::move(pickle);
}

void Ftrl::m__setstate__(const PKArgs& args) {
  m__dealloc__();
  dtft = new std::vector<dtftptr>;
  py::otuple pickle = args[0].to_otuple();

  // Set labels and initialize classifiers, this has to be done first.
  labels = pickle[4].to_pylist();
  init_dtft(dt::Ftrl::default_params);

  // Set FTRL parameters
  set_params_tuple(pickle[0]);

  // Set model weights
  set_model(pickle[1]);

  // Set feature importance info
  if (pickle[2].is_tuple()) {
    py::otuple fi_tuple = pickle[2].to_otuple();
    for (size_t i = 0; i < fi_tuple.size(); ++i) {
      (*dtft)[i]->set_fi(fi_tuple[i].to_frame()->copy());
    }
  }

  // Set feature names
  if (pickle[3].is_frame()) {
    feature_names = pickle[3].to_frame()->copy();
  }
  // Set regression type
  reg_type = static_cast<RegType>(pickle[5].to_int32());

}


} // namespace py
