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
#include "extras/py_ftrl.h"
#include <extras/py_validator.h>

namespace py {


PKArgs Ftrl::Type::args___init__(1, 0, 7, false, false,
                                 {"params", "alpha", "beta", "lambda1",
                                 "lambda2", "d", "n_epochs", "inter"},
                                 "__init__", nullptr);

static const char* doc_alpha    = "`alpha` in per-coordinate FTRL-Proximal algorithm";
static const char* doc_beta     = "`beta` in per-coordinate FTRL-Proximal algorithm";
static const char* doc_lambda1  = "L1 regularization parameter";
static const char* doc_lambda2  = "L2 regularization parameter";
static const char* doc_d        = "Number of bins to be used for the hashing trick";
static const char* doc_n_epochs = "Number of epochs to train a model";
static const char* doc_inter    = "If feature interactions to be used or not";

static onamedtupletype& _get_params_namedtupletype() {
  static onamedtupletype ntt(
    "FtrlParams",
    "FTRL model parameters",
    {{"alpha",    doc_alpha},
     {"beta",     doc_beta},
     {"lambda1",  doc_lambda1},
     {"lambda2",  doc_lambda2},
     {"d",        doc_d},
     {"n_epochs", doc_n_epochs},
     {"inter",    doc_inter}}
  );
  return ntt;
}


void Ftrl::m__init__(PKArgs& args) {
  dt::FtrlParams dt_params = dt::Ftrl::params_default;

  bool defined_params   = !args[0].is_none_or_undefined();
  bool defined_alpha    = !args[1].is_none_or_undefined();
  bool defined_beta     = !args[2].is_none_or_undefined();
  bool defined_lambda1  = !args[3].is_none_or_undefined();
  bool defined_lambda2  = !args[4].is_none_or_undefined();
  bool defined_d        = !args[5].is_none_or_undefined();
  bool defined_n_epochs = !args[6].is_none_or_undefined();
  bool defined_inter    = !args[7].is_none_or_undefined();

  py::Validator<double> dvalidator;
  py::Validator<uint64_t> uvalidator;

  if (defined_params) {
    if (defined_alpha || defined_beta || defined_lambda1 || defined_lambda2 ||
        defined_d || defined_n_epochs || defined_inter) {
      throw TypeError() << "You can either pass all the parameters with "
            << "`params` or any of the individual parameters with `alpha`, "
            << "`beta`, `lambda1`, `lambda2`, `d`, `n_epochs` or `inter` to "
            << "Ftrl constructor, but not both at the same time";
    }
    py::otuple py_params = args[0].to_otuple();
    py::oobj py_alpha = py_params.get_attr("alpha");
    py::oobj py_beta = py_params.get_attr("beta");
    py::oobj py_lambda1 = py_params.get_attr("lambda1");
    py::oobj py_lambda2 = py_params.get_attr("lambda2");
    py::oobj py_d = py_params.get_attr("d");
    py::oobj py_n_epochs = py_params.get_attr("n_epochs");
    py::oobj py_inter = py_params.get_attr("inter");

    dt_params.alpha = py_alpha.to_double();
    dt_params.beta = py_beta.to_double();
    dt_params.lambda1 = py_lambda1.to_double();
    dt_params.lambda2 = py_lambda2.to_double();
    dt_params.d = static_cast<uint64_t>(py_d.to_size_t());
    dt_params.n_epochs = py_n_epochs.to_size_t();
    dt_params.inter = py_inter.to_bool_strict();

    dvalidator.check_positive(dt_params.alpha, py_alpha);
    dvalidator.check_not_negative(dt_params.beta, py_beta);
    dvalidator.check_not_negative(dt_params.lambda1, py_lambda1);
    dvalidator.check_not_negative(dt_params.lambda2, py_lambda2);
    uvalidator.check_positive(dt_params.d, py_d);

  } else {

    if (defined_alpha) {
      dt_params.alpha = args[1].to_double();
      dvalidator.check_positive(dt_params.alpha, args[1]);
    }

    if (defined_beta) {
      dt_params.beta = args[2].to_double();
      dvalidator.check_not_negative(dt_params.beta, args[2]);
    }

    if (defined_lambda1) {
      dt_params.lambda1 = args[3].to_double();
      dvalidator.check_not_negative(dt_params.lambda1, args[3]);
    }

    if (defined_lambda2) {
      dt_params.lambda2 = args[4].to_double();
      dvalidator.check_not_negative(dt_params.lambda2, args[4]);
    }

    if (defined_d) {
      dt_params.d = static_cast<uint64_t>(args[5].to_size_t());
      uvalidator.check_positive(dt_params.d, args[5]);
    }

    if (defined_n_epochs) {
      dt_params.n_epochs = args[6].to_size_t();
    }

    if (defined_inter) {
      dt_params.inter = args[7].to_bool_strict();
    }
  }

  dtft = new dt::Ftrl(dt_params);
}


void Ftrl::m__dealloc__() {
  delete dtft;
}


const char* Ftrl::Type::classname() {
  return "datatable.core.Ftrl";
}


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
d : int
    Number of bins to be used after the hashing trick.
n_epochs : int
    Number of epochs to train for.
inter : bool
    If feature interactions to be used or not.
)";
}


void Ftrl::Type::init_methods_and_getsets(Methods& mm, GetSetters& gs) {
  gs.add<&Ftrl::get_model, &Ftrl::set_model>(
    "model",
    "Frame having two columns, i.e. `z` and `n`, and `d` rows,\n"
    "where `d` is a number of bins set for modeling. Both column types\n"
    "must be `float64`"
  );

  gs.add<&Ftrl::get_params, &Ftrl::set_params>(
    "params",
    "FTRL model parameters"
  );

  gs.add<&Ftrl::get_colnames_hashes>(
    "colnames_hashes",
    "Column name hashes.\n"
  );

  gs.add<&Ftrl::get_alpha, &Ftrl::set_alpha>("alpha", doc_alpha);
  gs.add<&Ftrl::get_beta, &Ftrl::set_beta>("beta", doc_beta);
  gs.add<&Ftrl::get_lambda1, &Ftrl::set_lambda1>("lambda1", doc_lambda1);
  gs.add<&Ftrl::get_lambda2, &Ftrl::set_lambda2>("lambda2", doc_lambda2);
  gs.add<&Ftrl::get_d, &Ftrl::set_d>("d", doc_d);
  gs.add<&Ftrl::get_n_epochs, &Ftrl::set_n_epochs>("n_epochs", doc_n_epochs);
  gs.add<&Ftrl::get_inter, &Ftrl::set_inter>("inter", doc_inter);

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
----------
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

  if (dt_y->columns[0]->stype() != SType::BOOL ) {
    throw ValueError() << "Target column must be of a `bool` type";
  }

  if (dt_X->nrows != dt_y->nrows) {
    throw ValueError() << "Target column must have the same number of rows "
                       << "as the training frame";
  }

  dtft->fit(dt_X, dt_y);
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

  if (!dtft->is_trained()) {
    throw ValueError() << "Cannot make any predictions, train or set "
                       << "the model first";
  }

  size_t n_features = dtft->get_n_features();
  if (dt_X->ncols != n_features && n_features != 0) {
    throw ValueError() << "Can only predict on a frame that has " << n_features
                       << " column" << (n_features == 1? "" : "s")
                       << ", i.e. has the same number of features as "
                          "was used for model training";
  }

  DataTable* dt_y = dtft->predict(dt_X).release();
  py::oobj df_y = py::oobj::from_new_reference(
                         py::Frame::from_datatable(dt_y)
                  );

  return df_y;
}


NoArgs Ftrl::Type::args_reset("reset",
R"(reset(self)
--

Reset an FTRL model, i.e. initialize all the weights to zero.

Parameters
----------
    None

Returns
-------
    None
)");


void Ftrl::reset(const NoArgs&) {
  dtft->reset_model();
}


/*
*  Getters and setters.
*/
oobj Ftrl::get_model() const {
  if (dtft->is_trained()) {
    DataTable* dt_model = dtft->get_model();
    py::oobj df_model = py::oobj::from_new_reference(
                          py::Frame::from_datatable(dt_model)
                        );
    return df_model;
  } else {
    return py::None();
  }
}


oobj Ftrl::get_params() const {
  py::onamedtuple params(_get_params_namedtupletype());
  params.set(0, get_alpha());
  params.set(1, get_beta());
  params.set(2, get_lambda1());
  params.set(3, get_lambda2());
  params.set(4, get_d());
  params.set(5, get_n_epochs());
  params.set(6, get_inter());
  return std::move(params);
}



oobj Ftrl::get_alpha() const {
  return py::ofloat(dtft->get_alpha());
}


oobj Ftrl::get_beta() const {
  return py::ofloat(dtft->get_beta());
}


oobj Ftrl::get_lambda1() const {
  return py::ofloat(dtft->get_lambda1());
}


oobj Ftrl::get_lambda2() const {
  return py::ofloat(dtft->get_lambda2());
}


oobj Ftrl::get_d() const {
  return py::oint(static_cast<size_t>(dtft->get_d()));
}


oobj Ftrl::get_n_epochs() const {
  return py::oint(dtft->get_n_epochs());
}


oobj Ftrl::get_inter() const {
  return dtft->get_inter()? True() : False();
}


oobj Ftrl::get_colnames_hashes() const {
  if (dtft->is_trained()) {
    size_t n_features = dtft->get_n_features();
    py::otuple py_colnames_hashes(n_features);
    std::vector<uint64_t> colnames_hashes = dtft->get_colnames_hashes();
    for (size_t i = 0; i < n_features; ++i) {
      size_t h = static_cast<size_t>(colnames_hashes[i]);
      py_colnames_hashes.set(i, py::oint(h));
    }
    return std::move(py_colnames_hashes);
  } else {
    return py::None();
  }
}


void Ftrl::set_model(robj model) {
  DataTable* dt_model_in = model.to_frame();

  // Reset model when it was assigned `None` in Python
  if (dt_model_in == nullptr) {
    if (dtft->is_trained()) dtft->reset_model();
    return;
  }

  const std::vector<std::string>& model_cols_in = dt_model_in->get_names();

  if (dt_model_in->nrows != dtft->get_d() || dt_model_in->ncols != 2) {
    throw ValueError() << "FTRL model frame must have " << dtft->get_d()
                       << " rows, and 2 columns, whereas your frame has "
                       << dt_model_in->nrows << " rows and "
                       << dt_model_in->ncols << " column"
                       << (dt_model_in->ncols == 1? "": "s");

  }

  if (model_cols_in != dt::Ftrl::model_cols) {
    throw ValueError() << "FTRL model frame must have columns named `z` and "
                       << "`n`, whereas your frame has the following column "
                       << "names: `" << model_cols_in[0]
                       << "` and `" << model_cols_in[1] << "`";
  }

  if (dt_model_in->columns[0]->stype() != SType::FLOAT64 ||
    dt_model_in->columns[1]->stype() != SType::FLOAT64) {
    throw ValueError() << "FTRL model frame must have both column types as "
                       << "`float64`, whereas your frame has the following "
                       << "column types: `"
                       << dt_model_in->columns[0]->stype()
                       << "` and `" << dt_model_in->columns[1]->stype() << "`";
  }

  if (has_negative_n(dt_model_in)) {
    throw ValueError() << "Values in column `n` cannot be negative";
  }

  dtft->set_model(dt_model_in);
}


void Ftrl::set_params(robj params) {
  set_alpha(params.get_attr("alpha"));
  set_beta(params.get_attr("beta"));
  set_lambda1(params.get_attr("lambda1"));
  set_lambda2(params.get_attr("lambda2"));
  set_d(params.get_attr("d"));
  set_n_epochs(params.get_attr("n_epochs"));
  set_inter(params.get_attr("inter"));
  // TODO: check that there are no unknown parameters
}


void Ftrl::set_alpha(robj py_alpha) {
  py::Validator<double> dvalidator;
  double alpha = py_alpha.to_double();
  dvalidator.check_positive(alpha, py_alpha);
  dtft->set_alpha(alpha);
}


void Ftrl::set_beta(robj py_beta) {
  py::Validator<double> dvalidator;
  double beta = py_beta.to_double();
  dvalidator.check_not_negative(beta, py_beta);
  dtft->set_beta(beta);
}


void Ftrl::set_lambda1(robj py_lambda1) {
  py::Validator<double> dvalidator;
  double lambda1 = py_lambda1.to_double();
  dvalidator.check_not_negative(lambda1, py_lambda1);
  dtft->set_lambda1(lambda1);
}


void Ftrl::set_lambda2(robj py_lambda2) {
  py::Validator<double> dvalidator;
  double lambda2 = py_lambda2.to_double();
  dvalidator.check_not_negative(lambda2, py_lambda2);
  dtft->set_lambda2(lambda2);
}


void Ftrl::set_d(robj py_d) {
  py::Validator<uint64_t> uvalidator;
  uint64_t d = static_cast<uint64_t>(py_d.to_size_t());
  uvalidator.check_positive(d, py_d);
  dtft->set_d(d);
}


void Ftrl::set_n_epochs(robj n_epochs) {
  size_t n_epochs_in = n_epochs.to_size_t();
  dtft->set_n_epochs(n_epochs_in);
}


void Ftrl::set_inter(robj inter) {
  bool inter_in = inter.to_bool_strict();
  dtft->set_inter(inter_in);
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

} // namespace py
