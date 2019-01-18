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
#include "extras/dt_ftrl.h"
#include "extras/murmurhash.h"

namespace dt {

/*
*  Set column names for `dt_model` and default parameter values.
*/
const std::vector<std::string> Ftrl::model_colnames = {"z", "n"};
const FtrlParams Ftrl::default_params = {0.005, 1.0, 0.0, 1.0,
                                         1000000, 1, false};

/*
*  Set up FTRL parameters and initialize weights.
*/
Ftrl::Ftrl(FtrlParams params_in) :
  z(nullptr),
  n(nullptr),
  fi(nullptr),
  params(params_in),
  ncols(0),
  nfeatures(0),
  model_trained(false)
{
}


/*
*  Make a prediction for an array of hashed features.
*/
double Ftrl::predict_row(const uint64ptr& x) {
  double wTx = 0;
  double l1 = params.lambda1;
  double ia = 1 / params.alpha;
  double rr = params.beta * ia + params.lambda2;
  for (size_t i = 0; i < nfeatures; ++i) {
    size_t j = x[i];
    double absw = std::max(std::abs(z[j]) - l1, 0.0) /
                  (std::sqrt(n[j]) * ia + rr);
    w[j] = -std::copysign(absw, z[j]);
    wTx += w[j];
    fi[i] += absw; // Update feature importance vector
  }
  return wTx;
}


/*
*  Update weights based on prediction `p` and the actual target `y`.
*/
void Ftrl::update(const uint64ptr& x, double p, double y) {
  double ia = 1 / params.alpha;
  double g = p - y;
  double gsq = g * g;

  for (size_t i = 0; i < nfeatures; ++i) {
    size_t j = x[i];
    double sigma = (std::sqrt(n[j] + gsq) - std::sqrt(n[j])) * ia;
    z[j] += g - sigma * w[j];
    n[j] += gsq;
  }
}


void Ftrl::create_model() {
  Column* col_z = Column::new_data_column(SType::FLOAT64, params.nbins);
  Column* col_n = Column::new_data_column(SType::FLOAT64, params.nbins);
  dt_model = dtptr(new DataTable({col_z, col_n}, model_colnames));
  w = doubleptr(new double[params.nbins]());
  reset_model();
}


void Ftrl::reset_model() {
  init_weights();
  if (z == nullptr || n == nullptr) return;
  std::memset(z, 0, params.nbins * sizeof(double));
  std::memset(n, 0, params.nbins * sizeof(double));
  model_trained = false;
}


void Ftrl::init_weights() {
  if (dt_model == nullptr) return;
  z = static_cast<double*>(dt_model->columns[0]->data_w());
  n = static_cast<double*>(dt_model->columns[1]->data_w());
}


void Ftrl::create_fi() {
  Column* col_fi = Column::new_data_column(SType::FLOAT64, nfeatures);
  dt_fi = dtptr(new DataTable({col_fi}, {"feature_importance"}));
  reset_fi();
}


void Ftrl::init_fi() {
  if (dt_fi == nullptr) return;
  fi = static_cast<double*>(dt_fi->columns[0]->data_w());
}


void Ftrl::reset_fi() {
  init_fi();
  if (fi == nullptr) return;
  std::memset(fi, 0, nfeatures * sizeof(double));
}


void Ftrl::define_features(size_t ncols_in) {
  ncols = ncols_in;
  size_t n_inter_features = (params.interactions)? ncols * (ncols - 1) / 2 : 0;
  nfeatures = ncols + n_inter_features;
}


void Ftrl::create_hashers(const DataTable* dt) {
  hashers.clear();
  hashers.reserve(ncols);

  for (size_t i = 0; i < ncols; ++i) {
    Column* col = dt->columns[i];
    hashers.push_back(create_colhasher(col));
  }

  // Also, pre-hash column names.
  // TODO: if we stick to default column names, like `C*`,
  // this may only be necessary, when number of features increases.
  const std::vector<std::string>& c_names = dt->get_names();
  colnames_hashes.clear();
  colnames_hashes.reserve(ncols);
  for (size_t i = 0; i < ncols; i++) {
    uint64_t h = hash_murmur2(c_names[i].c_str(),
                             c_names[i].length() * sizeof(char),
                             0);
    colnames_hashes.push_back(h);
  }
}


hashptr Ftrl::create_colhasher(const Column* col) {
  SType stype = col->stype();
  switch (stype) {
    case SType::BOOL:    return hashptr(new HashBool(col));
    case SType::INT8:    return hashptr(new HashInt<int8_t>(col));
    case SType::INT16:   return hashptr(new HashInt<int16_t>(col));
    case SType::INT32:   return hashptr(new HashInt<int32_t>(col));
    case SType::INT64:   return hashptr(new HashInt<int64_t>(col));
    case SType::FLOAT32: return hashptr(new HashFloat<float>(col));
    case SType::FLOAT64: return hashptr(new HashFloat<double>(col));
    case SType::STR32:   return hashptr(new HashString<uint32_t>(col));
    case SType::STR64:   return hashptr(new HashString<uint64_t>(col));
    default:             throw TypeError() << "Cannot hash column of type "
                                            << stype;
  }
}


/*
*  Hash each element of the datatable row, do feature interactionsaction if requested.
*/
void Ftrl::hash_row(uint64ptr& x, size_t row) {
  for (size_t i = 0; i < ncols; ++i) {
    // Hash a value adding a column name hash to it, so that the same value
    // in different columns results in different hashes.
    x[i] = (hashers[i]->hash(row) + colnames_hashes[i]) % params.nbins;
  }

  // Do feature interactions if required. We may also want to test
  // just a simple `h = x[i+1] + x[j+1]` approach here.
  size_t count = 0;
  if (params.interactions) {
    for (size_t i = 0; i < ncols - 1; ++i) {
      for (size_t j = i + 1; j < ncols; ++j) {
        std::string s = std::to_string(x[i+1]) + std::to_string(x[j+1]);
        uint64_t h = hash_murmur2(s.c_str(), s.length() * sizeof(char), 0);
        x[ncols + count] = h % params.nbins;
        count++;
      }
    }
  }
}


bool Ftrl::is_dt_valid(const dtptr& dt, size_t nrows_in, size_t ncols_in) {
  if (dt == nullptr) return false;
  // Normally, this exception should never be thrown.
  // For the moment, the only purpose of this check is to make sure we didn't
  // do something wrong to the model and feature importance datatables.
  if (dt->ncols != ncols_in) {
    throw ValueError() << "Datatable should have " << ncols_in << "column"
                       << (ncols_in == 1? "" : "s") << ", got: " << dt->ncols;
  }
  if (dt->nrows != nrows_in) return false;
  return true;
}


bool Ftrl::is_trained() {
  return model_trained;
}


/**
 * Calculate logloss based on a prediction `p` and the actual target `y`.
 *     log-loss = -log(p * y + (1 - p) * (1 - y))
 */
double Ftrl::logloss(double p, bool y) {
  double epsilon = std::numeric_limits<double>::epsilon();
  p = std::max(std::min(p, 1 - epsilon), epsilon);
  return -std::log(p * (2*y - 1) + 1 - y);
}


/*
*  Get a shallow copy of an FTRL model if available.
*/
DataTable* Ftrl::get_model() {
  if (dt_model != nullptr) {
    return dt_model->copy();
  } else {
    return nullptr;
  }
}


/*
*  Get a shallow copy of a feature importance datatable if available.
*/
DataTable* Ftrl::get_fi() {
  if (dt_fi != nullptr) {
    return dt_fi->copy();
  } else {
    return nullptr;
  }
}


/*
*  Other getters and setters.
*  Here we assume that all the validation for setters is done in `py_ftrl.cc`.
*/
std::vector<uint64_t> Ftrl::get_colnames_hashes() {
  return colnames_hashes;
}


size_t Ftrl::get_ncols() {
  return ncols;
}


size_t Ftrl::get_nfeatures() {
  return nfeatures;
}


double Ftrl::get_alpha() {
  return params.alpha;
}


double Ftrl::get_beta() {
  return params.beta;
}


double Ftrl::get_lambda1() {
  return params.lambda1;
}


double Ftrl::get_lambda2() {
  return params.lambda2;
}


uint64_t Ftrl::get_nbins() {
  return params.nbins;
}


bool Ftrl::get_interactions() {
  return params.interactions;
}


size_t Ftrl::get_nepochs() {
  return params.nepochs;
}


FtrlParams Ftrl::get_params() {
  return params;
}


/*
*  Set an FTRL model, assuming all the validation is done in `py_ftrl.cc`
*/
void Ftrl::set_model(DataTable* dt_model_in) {
  dt_model = dtptr(dt_model_in->copy());
  set_nbins(dt_model->nrows);
  w = doubleptr(new double[params.nbins]());
  init_weights();
  ncols = 0;
  nfeatures = 0;
  model_trained = true;
}


void Ftrl::set_fi(DataTable* dt_fi_in) {
  dt_fi = dtptr(dt_fi_in->copy());
  nfeatures = dt_fi->nrows;
  init_fi();
}


void Ftrl::set_alpha(double alpha_in) {
  params.alpha = alpha_in;
}


void Ftrl::set_beta(double beta_in) {
  params.beta = beta_in;
}


void Ftrl::set_lambda1(double lambda1_in) {
  params.lambda1 = lambda1_in;
}


void Ftrl::set_lambda2(double lambda2_in) {
  params.lambda2 = lambda2_in;
}


void Ftrl::set_nbins(uint64_t nbins_in) {
  params.nbins = nbins_in;
}


void Ftrl::set_interactions(bool interactions_in) {
  params.interactions = interactions_in;
}


void Ftrl::set_nepochs(size_t nepochs_in) {
  params.nepochs = nepochs_in;
}

} // namespace dt

