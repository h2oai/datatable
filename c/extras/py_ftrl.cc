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


oobj Ftrl::fit(const PKArgs& args) {
  DataTable* dt_train = args[0].to_frame();

  DataTable* dt_model = fm->fit(dt_train).release();
  py::Frame* df_model = py::Frame::from_datatable(dt_model);

  return df_model;
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
  DataTable* dt_test = args[0].to_frame();
  DataTable* dt_target = fm->predict(dt_test).release();
  py::Frame* df_target = py::Frame::from_datatable(dt_target);

  return df_target;
}


const char* Ftrl::Type::classname() {
  return "datatable.core.Ftrl";
}


const char* Ftrl::Type::classdoc() {
  return
    "Follow the Regularized Leader (FTRL) model.\n"
    "\n"
    "See this reference for more details:\n"
    "https://www.eecs.tufts.edu/~dsculley/papers/ad-click-prediction.pdf\n";
}

void Ftrl::m__dealloc__() {
  delete fm;
}

} // namespace py
