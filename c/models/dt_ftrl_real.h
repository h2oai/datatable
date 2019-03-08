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
#ifndef dt_MODELS_FTRL_REAL_h
#define dt_MODELS_FTRL_REAL_h
#include "str/py_str.h"
#include "utils/parallel.h"
#include "models/column_hasher.h"
#include "models/utils.h"
#include "models/dt_ftrl.h"


namespace dt {


/*
* Class template that implements all the virtual methods declared in dt::Ftrl.
*/
template <typename T /* float or double */>
class FtrlReal : public dt::Ftrl {
  private:
    // Model datatable of shape (nbins, 2 * nlabels),
    // a vector of weight pointers, and the model type.
    dtptr dt_model;
    std::vector<T*> z, n;
    FtrlModelType model_type;

    // Feature importances datatable of shape (nfeatures, 2),
    // where the first column contains feature names and the second one
    // feature importance values.
    dtptr dt_fi;

    // FTRL model parameters provided to constructor
    FtrlParams params;

    // Individual parameters converted to T type
    T alpha;
    T beta;
    T lambda1;
    T lambda2;
    uint64_t nbins;
    size_t nepochs;
    bool interactions;
    size_t : 56;

    // Vector of labels that is automatically constructed based on the target
    strvec labels;

    // Number of columns in a training datatable,
    // and the total number of features that also includes interactions
    size_t dt_X_ncols;
    size_t nfeatures;

    // Vector of hashed column names
    std::vector<uint64_t> colnames_hashes;

    // Pointers to training and validation data. The data is not owned.
    const DataTable* dt_X;
    const DataTable* dt_y;
    const DataTable* dt_X_val;
    const DataTable* dt_y_val;

    // Fitting methods
    template <typename U>
    double fit(double, T(*)(T), T(*)(T,U));
    template <typename U>
    double fit_regression(double);
    double fit_binomial(double);
    double fit_multinomial(double);
    template <typename U>
    void update(const uint64ptr&, const tptr<T>&, T, U, size_t);

    // Predicting methods
    template <typename F>
    T predict_row(const uint64ptr&, tptr<T>&, size_t, F);

    // Hashing methods
    std::vector<hasherptr> create_hashers(const DataTable*);
    static hasherptr create_hasher(const Column*);
    void hash_row(uint64ptr&, std::vector<hasherptr>&, size_t);

    // Model helper methods
    void create_model();
    void adjust_model();
    void init_model();
    void init_weights();

    // Feature importance helper methods
    void create_fi(const DataTable*);
    void init_fi();
    void define_features(size_t);
    void normalize_rows(dtptr&);

    // Prediction helper methods
    dtptr create_dt_p(size_t);

    // Other helpers
    static bool is_dt_valid(const dtptr&, size_t, size_t);
    template <typename U>
    void fill_ri_data(const DataTable*,
                      std::vector<const RowIndex>&,
                      std::vector<const U*>&);

  public:
    FtrlReal(FtrlParams);

    // Main fitting method
    double dispatch_fit(const DataTable*, const DataTable*,
                        const DataTable*, const DataTable*, double) override;

    // Main predicting method
    dtptr predict(const DataTable*) override;

    // Model methods
    void reset() override;
    bool is_trained() override;

    // Getters
    DataTable* get_model() override;
    DataTable* get_fi(bool normalize = true) override;
    FtrlModelType get_model_type() override;
    size_t get_nfeatures() override;
    size_t get_dt_X_ncols() override;
    std::vector<uint64_t> get_colnames_hashes() override;
    double get_alpha() override;
    double get_beta() override;
    double get_lambda1() override;
    double get_lambda2() override;
    uint64_t get_nbins() override;
    size_t get_nepochs() override;
    bool get_interactions() override;
    bool get_double_precision() override;
    FtrlParams get_params() override;
    strvec get_labels() override;

    // Setters
    void set_model(DataTable*) override;
    void set_fi(DataTable*) override;
    void set_model_type(FtrlModelType) override;
    void set_alpha(double) override;
    void set_beta(double) override;
    void set_lambda1(double) override;
    void set_lambda2(double) override;
    void set_nbins(uint64_t) override;
    void set_nepochs(size_t) override;
    void set_interactions(bool) override;
    void set_double_precision(bool) override;
    void set_labels(strvec) override;
};


extern template class FtrlReal<float>;
extern template class FtrlReal<double>;

} // namespace dt

#endif
