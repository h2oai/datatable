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

PKArgs Ftrl::Type::args___init__(0, 0, 9, false, false,
                                 {"a", "b", "l1", "l2", "d", "n_epochs",
                                 "inter", "hash_type", "seed"},
                                 "__init__", nullptr);

void Ftrl::m__init__(PKArgs& args) {
  FtrlModelParams fmp = FtrlModel::fmp_default;

  if (!(args[0].is_undefined() || args[0].is_none())) {
    fmp.a = args[0].to_double();
  }

  if (!(args[1].is_undefined() || args[1].is_none())) {
    fmp.b = args[1].to_double();
  }

  if (!(args[2].is_undefined() || args[2].is_none())) {
    fmp.l1 = args[2].to_double();
  }

  if (!(args[3].is_undefined() || args[3].is_none())) {
    fmp.l2 = args[3].to_double();
  }

  if (!(args[4].is_undefined() || args[4].is_none())) {
    fmp.d = static_cast<uint64_t>(args[4].to_size_t());
  }

  if (!(args[5].is_undefined() || args[5].is_none())) {
    fmp.n_epochs = args[5].to_size_t();
  }

  if (!(args[6].is_undefined() || args[6].is_none())) {
    fmp.inter = args[6].to_bool_strict();
  }

  if (!(args[7].is_undefined() || args[7].is_none())) {
    fmp.hash_type = static_cast<unsigned int>(args[7].to_size_t());
  }

  if (!(args[8].is_undefined() || args[8].is_none())) {
    fmp.seed = static_cast<unsigned int>(args[8].to_size_t());
  }

  fm = new FtrlModel(fmp);
}


void Ftrl::m__dealloc__() {
  delete fm;
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
a : float
    `alpha` in per-coordinate learning rate formula.
b : float
    `beta` in per-coordinate learning rate formula.
l1 : float
    L1 regularization parameter.
l2 : float
    L2 regularization parameter.
d : int
    Number of bins to be used after the hashing trick. 
n_epochs : int
    Number of epochs to train for.
inter : bool
    If feature interactions to be used or not.
hash_type : int
    Hashing method to use for strings:
    `0` - std::hash;
    `1` - Murmur2;
    `2` - Murmur3.
seed: int
    Seed to be used for Murmur hash functions.
)";
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
  gs.add<&Ftrl::get_hash_type, &Ftrl::set_hash_type>("hash_type", "Hashing method to use for strings.\n"
    "`0` - std::hash;\n"
    "`1` - Murmur2;\n"
    "`2` - Murmur3.\n");
  gs.add<&Ftrl::get_seed, &Ftrl::set_seed>("seed", "Seed to be used for Murmur hash functions.\n");

  mm.add<&Ftrl::fit, args_fit>();
  mm.add<&Ftrl::predict, args_predict>();
  mm.add<&Ftrl::reset, args_reset>();
}


PKArgs Ftrl::Type::args_fit(1, 0, 0, false, false, {"frame"}, "fit",
R"(fit(self, frame)
--

Train an FTRL model on a dataset.

Parameters
----------
frame: Frame
    Frame to be trained on, last column is treated as `target`.

Returns
----------
    None
)");


void Ftrl::fit(const PKArgs& args) {
  DataTable* dt_train = args[0].to_frame();
  fm->fit(dt_train);
}


PKArgs Ftrl::Type::args_predict(1, 0, 0, false, false, {"frame"}, "predict",
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
  if (fm->is_trained()) {
    DataTable* dt_test = args[0].to_frame();
    DataTable* dt_target = fm->predict(dt_test).release();
    py::oobj df_target = py::oobj::from_new_reference(py::Frame::from_datatable(dt_target));
    return df_target;
  } else {
    throw ValueError() << "Cannot make any predictions, because the model was not trained";
  }
}


PKArgs Ftrl::Type::args_reset(0, 0, 0, false, false, {}, "reset",
R"(reset(self)
--

Reset an FTRL model.

Parameters
----------
    None

Returns
----------
    None
)");


void Ftrl::reset(const PKArgs&) {
  fm->init_model();
}

/*
*  Getter and setter for the model datatable.
*/
oobj Ftrl::get_model() const {
  if (fm->is_trained()) {
    DataTable* dt_model = fm->get_model();
    py::oobj df_model = py::oobj::from_new_reference(py::Frame::from_datatable(dt_model));
    return df_model;
  } else {
    return py::None();
  }
}


void Ftrl::set_model(robj model) {
  DataTable* dt_model_in = model.to_frame();
  const std::vector<std::string>& model_cols_in = dt_model_in->get_names();

  if (dt_model_in->nrows != fm->get_d() || dt_model_in->ncols != 2) {
    throw ValueError() << "FTRL model frame must have " << fm->get_d() << " rows,"
                       << "and 2 columns, whereas your frame has " << dt_model_in->nrows
                       << " rows and " << dt_model_in->ncols << " columns";
  }

  if (model_cols_in != FtrlModel::model_cols) {
    throw ValueError() << "FTRL model frame must have columns named `z` and `n`,"
                       << "whereas your frame has the following column names `" << model_cols_in[0]
                       << "` and `" << model_cols_in[1] << "`";
  }

  if (dt_model_in->columns[0]->stype() != SType::FLOAT64 ||
    dt_model_in->columns[1]->stype() != SType::FLOAT64) {
    throw ValueError() << "FTRL model frame must have both column types as `float64`, "
                       << "whereas your frame has the following column types: `"
                       << dt_model_in->columns[0]->stype()
                       << "` and `" << dt_model_in->columns[1]->stype() << "`";
  }

  fm->set_model(dt_model_in);
}


/*
*  All other getters and setters.
*/
oobj Ftrl::get_a() const {
  return py::ofloat(fm->get_a());
}


oobj Ftrl::get_b() const {
  return py::ofloat(fm->get_b());
}


oobj Ftrl::get_l1() const {
  return py::ofloat(fm->get_l1());
}


oobj Ftrl::get_l2() const {
  return py::ofloat(fm->get_l2());
}


oobj Ftrl::get_d() const {
  return py::oint(static_cast<size_t>(fm->get_d()));
}


oobj Ftrl::get_n_epochs() const {
  return py::oint(fm->get_n_epochs());
}


oobj Ftrl::get_inter() const {
  return py::oint(static_cast<size_t>(fm->get_inter()));
}


oobj Ftrl::get_hash_type() const {
  return py::oint(static_cast<size_t>(fm->get_hash_type()));
}


oobj Ftrl::get_seed() const {
  return py::oint(static_cast<size_t>(fm->get_seed()));
}


void Ftrl::set_a(robj a) {
  if (!a.is_numeric()) {
    throw TypeError() << "`a` must be numeric, not "
        << a.typeobj();
  }
  fm->set_a(a.to_double());
}


void Ftrl::set_b(robj b) {
  if (!b.is_numeric()) {
    throw TypeError() << "`b` must be numeric, not "
        << b.typeobj();
  }
  fm->set_b(b.to_double());
}


void Ftrl::set_l1(robj l1) {
  if (!l1.is_numeric()) {
    throw TypeError() << "`l1` must be numeric, not "
        << l1.typeobj();
  }
  fm->set_l1(l1.to_double());
}


void Ftrl::set_l2(robj l2) {
  if (!l2.is_numeric()) {
    throw TypeError() << "`l2` must be numeric, not "
        << l2.typeobj();
  }
  fm->set_l2(l2.to_double());
}


void Ftrl::set_d(robj d) {
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


void Ftrl::set_n_epochs(robj n_epochs) {
  if (!n_epochs.is_int()) {
    throw TypeError() << "`n_epochs` must be integer, not "
        << n_epochs.typeobj();
  }
  int64_t n_epochs_in = n_epochs.to_int64_strict();
  if (n_epochs_in < 0) {
    throw ValueError() << "`n_epochs` cannot be negative";
  }
  fm->set_n_epochs(static_cast<size_t>(n_epochs_in));
}


void Ftrl::set_inter(robj inter) {
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


void Ftrl::set_hash_type(robj hash_type) {
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


void Ftrl::set_seed(robj seed) {
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

} // namespace py
