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

namespace py {

PKArgs Ftrl::Type::args___init__(9, 0, 0, false, false,
                                 {"a", "b", "l1", "l2", "d", "n_epochs",
                                 "inter", "hash_type", "seed"},
                                 "__init__", nullptr);

void Ftrl::m__init__(PKArgs& args) {
  double a = args[0].to_double();
  double b = args[1].to_double();
  double l1 = args[2].to_double();
  double l2 = args[3].to_double();

  uint64_t d = static_cast<uint64_t>(args[4].to_size_t());
  size_t n_epochs = args[5].to_size_t();
  bool inter = args[6].to_bool_strict();
  unsigned int hash_type = static_cast<unsigned int>(args[7].to_size_t());
  unsigned int seed = static_cast<unsigned int>(args[8].to_size_t());

  fm = new FtrlModel(a, b, l1, l2, d, n_epochs, inter, hash_type, seed);
}


void Ftrl::Type::init_methods_and_getsets(Methods& mm, GetSetters& gs) {
  gs.add<&Ftrl::get_model, &Ftrl::set_model>("model",
    "Frame having two columns, i.e. `z` and `n`, and `d` rows,\n"
    "where `d` is a number of bins set for modeling. Both column types\n"
    "must be `FLOAT64`.\n"
    "NB: as the model trains, this frame will be changed in-place.\n");

  gs.add<&Ftrl::get_a, &Ftrl::set_a>("a", "`alpha` in per-coordinate learning rate formula.\n");
  gs.add<&Ftrl::get_b, &Ftrl::set_b>("b", "`beta` in per-coordinate learning rate formula.\n");
  gs.add<&Ftrl::get_l1, &Ftrl::set_l1>("l1", "L1 regularization parameter.\n");
  gs.add<&Ftrl::get_l2, &Ftrl::set_l2>("l2", "L2 regularization parameter.\n");
  gs.add<&Ftrl::get_d, &Ftrl::set_d>("d", "Number of bins to be used after the hashing trick.\n");
  gs.add<&Ftrl::get_n_epochs, &Ftrl::set_n_epochs>("n_epochs", "Number of epochs to train for.\n");
  gs.add<&Ftrl::get_inter, &Ftrl::set_inter>("inter", "If feature interactions to be used or not.\n");
  gs.add<&Ftrl::get_hash_type, &Ftrl::set_hash_type>("hash_type",
    "Hashing method to use for strings.\n"
    "`0` - std::hash;\n"
    "`1` - Murmur2;\n"
    "`2` - Murmur3.\n");
  gs.add<&Ftrl::get_seed, &Ftrl::set_seed>("seed", "Seed to be used for Murmur hash functions.\n");

  mm.add<&Ftrl::fit, args_fit>();
  mm.add<&Ftrl::predict, args_predict>();
}


PKArgs Ftrl::Type::args_fit(1, 0, 0, false, false, {"df_train"}, "fit",
R"(fit(self, frame)
--

Train an FTRL model on a dataset.

Parameters
----------
frame: Frame
    Frame to be trained on, last column is treated as `target`.

Returns
----------
    A new Frame of shape `(d, 2)` containing `z` and `n` model coefficients.
)");


void Ftrl::fit(const PKArgs& args) {
  if (!args[0].is_frame()) {
    throw TypeError() << "argument must be a frame, not "
        << args[0].typeobj();
  }
  DataTable* dt_train = args[0].to_frame();
  fm->fit(dt_train);
}


PKArgs Ftrl::Type::args_predict(1, 0, 0, false, false, {"df_test"}, "predict",
R"(predict(self, frame)
--

Make predictions for a dataset.

Parameters
----------
frame: Frame
    Frame of shape `(nrows, ncols)` to make predictions for. It must have one 
    column less than the training dataset.

Returns
----------
    A new `Frame` of shape `(nrows, 1)` with a prediction for each row.
)");


oobj Ftrl::predict(const PKArgs& args) {
  if (fm->get_model() == nullptr) {
    throw ValueError() << "There is no trained model in place.\n"
                       << "To make predictions, the model must be either trained first, or "
                       << "set.";
  }

  if (!args[0].is_frame()) {
    throw TypeError() << "argument must be a frame, not "
        << args[0].typeobj();
  }

  DataTable* dt_test = args[0].to_frame();
  DataTable* dt_target = fm->predict(dt_test).release();
  py::Frame* df_target = py::Frame::from_datatable(dt_target);

  return df_target;
}


void Ftrl::set_model(obj model) {
  if (!model.is_frame()) {
    throw TypeError() << "`model` must be a frame, not "
        << model.typeobj();
  }

  DataTable* dt_model_in = model.to_frame();
  const std::vector<std::string> model_cols_in = dt_model_in->get_names();
  if (dt_model_in->nrows == fm->get_d() && dt_model_in->ncols == 2 &&
    dt_model_in->columns[0]->stype() == SType::FLOAT64 &&
    dt_model_in->columns[1]->stype() == SType::FLOAT64 &&
    model_cols_in == FtrlModel::model_cols) {
    fm->set_model(dt_model_in);
  } else {
    throw ValueError() << "FTRL model frame must have " << fm->get_d() <<" rows, and" <<
                          "2 columns named `z` and `n`, " <<
                          "both columns must be of `FLOAT64` type.";
  }
}


oobj Ftrl::get_model(void) const {
  DataTable* dt_model = fm->get_model();
  if (dt_model == nullptr) {
    throw ValueError() << "There is no trained model available, train it first or set.";
  }
  py::Frame* df_model = py::Frame::from_datatable(dt_model);
  return df_model;
}


oobj Ftrl::get_a() const {
  return py::ofloat(fm->get_a());
}


void Ftrl::set_a(obj a) {
  if (!a.is_numeric()) {
    throw TypeError() << "`a` must be numeric, not "
        << a.typeobj();
  }
  fm->set_a(a.to_double());
}


oobj Ftrl::get_b() const {
  return py::ofloat(fm->get_b());
}


void Ftrl::set_b(obj b) {
  if (!b.is_numeric()) {
    throw TypeError() << "`b` must be numeric, not "
        << b.typeobj();
  }
  fm->set_b(b.to_double());
}


oobj Ftrl::get_l1() const {
  return py::ofloat(fm->get_l1());
}


void Ftrl::set_l1(obj l1) {
  if (!l1.is_numeric()) {
    throw TypeError() << "`l1` must be numeric, not "
        << l1.typeobj();
  }
  fm->set_l1(l1.to_double());
}


oobj Ftrl::get_l2() const {
  return py::ofloat(fm->get_l2());
}


void Ftrl::set_l2(obj l2) {
  if (!l2.is_numeric()) {
    throw TypeError() << "`l2` must be numeric, not "
        << l2.typeobj();
  }
  fm->set_l2(l2.to_double());
}


oobj Ftrl::get_d() const {
  return py::oint(static_cast<size_t>(fm->get_d()));
}


void Ftrl::set_d(obj d) {
  if (!d.is_int()) {
    throw TypeError() << "`d` must be integer, not "
        << d.typeobj();
  }
  int64_t d_in = d.to_int64_strict();
  if (d_in < 0) {
    throw ValueError() << "`d` cannot be negative";
  }
  fm->set_d(static_cast<uint64_t>(d_in));
}


oobj Ftrl::get_n_epochs() const {
  return py::oint(fm->n_epochs);
}


void Ftrl::set_n_epochs(obj n_epochs) {
  if (!n_epochs.is_int()) {
    throw TypeError() << "`n_epochs` must be integer, not "
        << n_epochs.typeobj();
  }
  int64_t n_epochs_in = n_epochs.to_int64_strict();
  if (n_epochs_in < 0) {
    throw ValueError() << "`n_epochs` cannot be negative";
  }
  fm->n_epochs = static_cast<size_t>(n_epochs_in);
}



oobj Ftrl::get_inter() const {
  return py::oint(static_cast<size_t>(fm->get_inter()));
}


void Ftrl::set_inter(obj inter) {
  if (!inter.is_int()) {
    throw TypeError() << "`inter` must be integer, not "
        << inter.typeobj();
  }
  int64_t inter_in = inter.to_int64_strict();
  if (inter_in != 0 && inter_in != 1) {
    throw ValueError() << "`inter` must be either `0` or `1`";
  }
  fm->set_d(static_cast<bool>(inter_in));
}


oobj Ftrl::get_hash_type() const {
  return py::oint(static_cast<size_t>(fm->get_hash_type()));
}


void Ftrl::set_hash_type(obj hash_type) {
  if (!hash_type.is_int()) {
    throw TypeError() << "`hash_type` must be integer, not "
        << hash_type.typeobj();
  }
  int64_t hash_type_in = hash_type.to_int64_strict();
  if (hash_type_in != 0 && hash_type_in != 1 && hash_type_in !=2) {
    throw ValueError() << "`hash_type_in` must be either `0` or `1` or `2`";
  }
  fm->set_hash_type(static_cast<unsigned int>(hash_type_in));
}


oobj Ftrl::get_seed() const {
  return py::oint(static_cast<size_t>(fm->get_seed()));
}


void Ftrl::set_seed(obj seed) {
  if (!seed.is_int()) {
    throw TypeError() << "`seed` must be integer, not "
        << seed.typeobj();
  }
  int32_t seed_in = seed.to_int32_strict();
  if (seed_in < 0) {
    throw ValueError() << "`seed` cannot be negative";
  }
  fm->set_seed(static_cast<unsigned int>(seed_in));
}



const char* Ftrl::Type::classname() {
  return "datatable.core.Ftrl";
}


const char* Ftrl::Type::classdoc() {
  return
    "Follow the Regularized Leader (FTRL) model.\n"
    "\n"
    "See this reference for more details:\n"
    "https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf";
}


void Ftrl::m__dealloc__() {
  delete fm;
}

} // namespace py
