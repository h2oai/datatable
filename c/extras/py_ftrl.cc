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
#include "extras/py_ftrl.h"
#include "frame/py_frame.h"
#include "python/float.h"
#include "python/int.h"
#include "python/tuple.h"

namespace py {

PKArgs PyFtrl::Type::args___init__(0, 0, 8, false, false,
                                   {"params", "alpha", "beta", "lambda1",
                                   "lambda2", "d", "n_epochs", "inter"},
                                   "__init__", nullptr);

std::vector<strpair> PyFtrl::params_fields_info = {
  strpair("alpha", "`alpha` in per-coordinate FTRL-Proximal algorithm"),
  strpair("beta", "`beta` in per-coordinate FTRL-Proximal algorithm"),
  strpair("lambda1", "L1 regularization parameter"),
  strpair("lambda2", "L1 regularization parameter"),
  strpair("d", "Number of bins to be used for the hashing trick"),
  strpair("n_epochs", "Number of epochs to train a model for"),
  strpair("inter", "Parameter that controls if feature interactions to be used "
                   "or not")
};
strpair PyFtrl::params_info = strpair("Params", "FTRL model parameters");
onamedtupletype PyFtrl::params_nttype(PyFtrl::params_info,
                                      PyFtrl::params_fields_info);


void PyFtrl::m__init__(PKArgs& args) {
  FtrlParams fp = Ftrl::params_default;

  bool defined_params = !args[0].is_none_or_undefined();
  bool defined_alpha = !args[1].is_none_or_undefined();
  bool defined_beta = !args[2].is_none_or_undefined();
  bool defined_lambda1 = !args[3].is_none_or_undefined();
  bool defined_lambda2 = !args[4].is_none_or_undefined();
  bool defined_d = !args[5].is_none_or_undefined();
  bool defined_n_epochs= !args[6].is_none_or_undefined();
  bool defined_inter = !args[7].is_none_or_undefined();

  if (defined_params) {
    if (!(defined_alpha || defined_beta || defined_lambda1 || defined_lambda2
        || defined_d || defined_n_epochs || defined_inter)) {

      py::otuple arg0_tuple = args[0].to_otuple();
      fp.alpha = arg0_tuple.get_attr("alpha").to_double();
      fp.beta = arg0_tuple.get_attr("beta").to_double();
      fp.lambda1 = arg0_tuple.get_attr("lambda1").to_double();
      fp.lambda2 = arg0_tuple.get_attr("lambda2").to_double();
      fp.d = static_cast<uint64_t>(arg0_tuple.get_attr("d").to_size_t());
      fp.n_epochs = arg0_tuple.get_attr("n_epochs").to_size_t();
      fp.inter = arg0_tuple.get_attr("inter").to_bool_strict();

    } else {
      throw TypeError() << "You can either pass all the parameters with "
            << "`params` or any of the individual parameters with `alpha`, "
            << "`beta`, `lambda1`, `lambda2`, `d`, `n_epochs` or `inter` to "
            << "Ftrl constructor, but not both at the same time";
    }
  } else {
    if (defined_alpha) {
      fp.alpha = args[1].to_double();
    }

    if (defined_beta) {
      fp.beta = args[2].to_double();
    }

    if (defined_lambda1) {
      fp.lambda1 = args[3].to_double();
    }

    if (defined_lambda2) {
      fp.lambda2 = args[4].to_double();
    }

    if (defined_d) {
      fp.d = static_cast<uint64_t>(args[5].to_size_t());
    }

    if (defined_n_epochs) {
      fp.n_epochs = args[6].to_size_t();
    }

    if (defined_inter) {
      fp.inter = args[7].to_bool_strict();
    }
  }

  ft = new Ftrl(fp);
}


void PyFtrl::m__dealloc__() {
  delete ft;
}


const char* PyFtrl::Type::classname() {
  return "datatable.core.Ftrl";
}


const char* PyFtrl::Type::classdoc() {
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


void PyFtrl::Type::init_methods_and_getsets(Methods& mm, GetSetters& gs) {
  gs.add<&PyFtrl::get_model, &PyFtrl::set_model>(
    "model",
    "Frame having two columns, i.e. `z` and `n`, and `d` rows,\n"
    "where `d` is a number of bins set for modeling. Both column types\n"
    "must be `FLOAT64`"
  );

  gs.add<&PyFtrl::get_params, &PyFtrl::set_params>(
    "params",
    "FTRL model parameters"
  );

  gs.add<&PyFtrl::get_colnames_hashes>(
    "colnames_hashes",
    "Column name hashes.\n"
  );

  gs.add<&PyFtrl::get_alpha, &PyFtrl::set_alpha>(
    params_fields_info[0].first,
    params_fields_info[0].second
  );

  gs.add<&PyFtrl::get_beta, &PyFtrl::set_beta>(
    params_fields_info[1].first,
    params_fields_info[1].second
  );

  gs.add<&PyFtrl::get_lambda1, &PyFtrl::set_lambda1>(
    params_fields_info[2].first,
    params_fields_info[2].second
  );

  gs.add<&PyFtrl::get_lambda2, &PyFtrl::set_lambda2>(
    params_fields_info[3].first,
    params_fields_info[3].second
  );

  gs.add<&PyFtrl::get_d, &PyFtrl::set_d>(
    params_fields_info[4].first,
    params_fields_info[4].second
  );

  gs.add<&PyFtrl::get_n_epochs, &PyFtrl::set_n_epochs>(
    params_fields_info[5].first,
    params_fields_info[5].second
  );


  gs.add<&PyFtrl::get_inter, &PyFtrl::set_inter>(
    params_fields_info[6].first,
    params_fields_info[6].second
  );

  mm.add<&PyFtrl::fit, args_fit>();
  mm.add<&PyFtrl::predict, args_predict>();
  mm.add<&PyFtrl::reset, args_reset>();
}


PKArgs PyFtrl::Type::args_fit(2, 0, 0, false, false, {"X", "y"}, "fit",
R"(fit(self, X, y)
--

Train an FTRL model on a dataset.

Parameters
----------
X: Frame
    Datatable frame of shape (nrows, ncols) to be trained on. 

y: Frame
    Datatable frame of shape (nrows, 1), i.e. the target column. 
    This column must have a `bool` type. 

Returns
----------
    None
)");


void PyFtrl::fit(const PKArgs& args) {
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
    throw ValueError() << "Target column must have the same number of rows"
                       << " as the training frame";
  }

  ft->fit(dt_X, dt_y);
}


PKArgs PyFtrl::Type::args_predict(1, 0, 0, false, false, {"X"}, "predict",
R"(predict(self, X)
--

Make predictions for a dataset.

Parameters
----------
X: Frame
    Datatable frame of shape (nrows, ncols) to make predictions for. 
    It must have the same number of columns as the training frame.

Returns
----------
    A new datatable frame of shape (nrows, 1) with a prediction 
    for each row of frame X.
)");


oobj PyFtrl::predict(const PKArgs& args) {
  if (args[0].is_undefined()) {
    throw ValueError() << "Frame to make predictions for is missing";
  }

  DataTable* dt_X = args[0].to_frame();

  if (dt_X == nullptr) return Py_None;

  if (!ft->is_trained()) {
    throw ValueError() << "Cannot make any predictions, because the model "
                       << "was not trained";
  }

  size_t n_features = ft->get_n_features();
  if (dt_X->ncols != n_features) {
    throw ValueError() << "Can only predict on a frame that has "<< n_features
                       << " column(s), i.e. has the same number of features as "
                       << "was used for model training";
  }

  DataTable* dt_y = ft->predict(dt_X).release();
  py::oobj df_y = py::oobj::from_new_reference(
                         py::Frame::from_datatable(dt_y)
                       );

  return df_y;
}


PKArgs PyFtrl::Type::args_reset(0, 0, 0, false, false, {}, "reset",
R"(reset(self)
--

Reset an FTRL model, i.e. initialize all the weights to zero.

Parameters
----------
    None

Returns
----------
    None
)");


void PyFtrl::reset(const PKArgs&) {
  ft->reset_model();
}


/*
*  Getters and setters.
*/
oobj PyFtrl::get_model() const {
  if (ft->is_trained()) {
    DataTable* dt_model = ft->get_model();
    py::oobj df_model = py::oobj::from_new_reference(
                          py::Frame::from_datatable(dt_model)
                        );
    return df_model;
  } else {
    return py::None();
  }
}


oobj PyFtrl::get_params() const {
  py::onamedtuple params(params_nttype);
  params.set(0, get_alpha());
  params.set(1, get_beta());
  params.set(2, get_lambda1());
  params.set(3, get_lambda2());
  params.set(4, get_d());
  params.set(5, get_n_epochs());
  params.set(6, get_inter());
  return std::move(params);
}



oobj PyFtrl::get_alpha() const {
  return py::ofloat(ft->get_alpha());
}


oobj PyFtrl::get_beta() const {
  return py::ofloat(ft->get_beta());
}


oobj PyFtrl::get_lambda1() const {
  return py::ofloat(ft->get_lambda1());
}


oobj PyFtrl::get_lambda2() const {
  return py::ofloat(ft->get_lambda2());
}


oobj PyFtrl::get_d() const {
  return py::oint(static_cast<size_t>(ft->get_d()));
}


oobj PyFtrl::get_n_epochs() const {
  return py::oint(ft->get_n_epochs());
}


oobj PyFtrl::get_inter() const {
  return ft->get_inter()? True() : False();
}


oobj PyFtrl::get_colnames_hashes() const {
  if (ft->is_trained()) {
    size_t n_features = ft->get_n_features();
    py::otuple py_colnames_hashes(n_features);
    std::vector<uint64_t> colnames_hashes = ft->get_colnames_hashes();
    for (size_t i = 0; i < n_features; ++i) {
      size_t h = static_cast<size_t>(colnames_hashes[i]);
      py_colnames_hashes.set(i, py::oint(h));
    }
    return std::move(py_colnames_hashes);
  } else {
    return py::None();
  }
}


void PyFtrl::set_model(robj model) {
  DataTable* dt_model_in = model.to_frame();

  // Reset model when it was assigned `None` in Python
  if (dt_model_in == nullptr) {
    if (ft->is_trained()) ft->reset_model();
    return;
  }

  const std::vector<std::string>& model_cols_in = dt_model_in->get_names();

  if (dt_model_in->nrows != ft->get_d() || dt_model_in->ncols != 2) {
    throw ValueError() << "FTRL model frame must have " << ft->get_d()
                       << " rows, and 2 columns, whereas your frame has "
                       << dt_model_in->nrows << " rows and "
                       << dt_model_in->ncols << " column(s)";

  }

  if (model_cols_in != Ftrl::model_cols) {
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

  auto c_double = static_cast<RealColumn<double>*>(dt_model_in->columns[1]);
  if (c_double->min() < 0) {
    throw ValueError() << "Values in column `n` cannot be negative";
  }

  ft->set_model(dt_model_in);
}


void PyFtrl::set_params(robj params) {
  set_alpha(params.get_attr("alpha"));
  set_beta(params.get_attr("beta"));
  set_lambda1(params.get_attr("lambda1"));
  set_lambda2(params.get_attr("lambda2"));
  set_d(params.get_attr("d"));
  set_n_epochs(params.get_attr("n_epochs"));
  set_inter(params.get_attr("inter"));
  // TODO: check that there are no unknown parameters
}


void PyFtrl::set_alpha(robj alpha_in) {
  double alpha = alpha_in.to_double();
  if (alpha <= 0) {
    throw ValueError() << "Parameter `alpha` must be positive";
  }
  ft->set_alpha(alpha);
}


void PyFtrl::set_beta(robj beta) {
  ft->set_beta(beta.to_double());
}


void PyFtrl::set_lambda1(robj lambda1) {
  ft->set_lambda1(lambda1.to_double());
}


void PyFtrl::set_lambda2(robj lambda2) {
  ft->set_lambda2(lambda2.to_double());
}


void PyFtrl::set_d(robj d) {
  int64_t d_in = d.to_int64_strict();
  if (d_in <= 0) {
    throw ValueError() << "Parameter `d` must be positive";
  }
  ft->set_d(static_cast<uint64_t>(d_in));
}


void PyFtrl::set_n_epochs(robj n_epochs) {
  int64_t n_epochs_in = n_epochs.to_int64_strict();
  if (n_epochs_in < 0) {
    throw ValueError() << "Parameter `n_epochs` cannot be negative";
  }
  ft->set_n_epochs(static_cast<size_t>(n_epochs_in));
}


void PyFtrl::set_inter(robj inter) {
  bool inter_in = inter.to_bool_strict();
  ft->set_inter(inter_in);
}


} // namespace py
