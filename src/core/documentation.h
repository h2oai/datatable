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
#ifndef dt_DOCUMENTATION_h
#define dt_DOCUMENTATION_h
namespace dt {

extern const char* doc_dt_as_type;
extern const char* doc_dt_by;
extern const char* doc_dt_cbind;
extern const char* doc_dt_corr;
extern const char* doc_dt_count;
extern const char* doc_dt_countna;
extern const char* doc_dt_cov;
extern const char* doc_dt_cut;
extern const char* doc_dt_first;
extern const char* doc_dt_fread;
extern const char* doc_dt_ifelse;
extern const char* doc_dt_init_styles;
extern const char* doc_dt_intersect;
extern const char* doc_dt_iread;
extern const char* doc_dt_join;
extern const char* doc_dt_last;
extern const char* doc_dt_max;
extern const char* doc_dt_mean;
extern const char* doc_dt_median;
extern const char* doc_dt_min;
extern const char* doc_dt_qcut;
extern const char* doc_dt_rbind;
extern const char* doc_dt_sd;
extern const char* doc_dt_setdiff;
extern const char* doc_dt_shift;
extern const char* doc_dt_sort;
extern const char* doc_dt_split_into_nhot;
extern const char* doc_dt_sum;
extern const char* doc_dt_symdiff;
extern const char* doc_dt_union;
extern const char* doc_dt_unique;
extern const char* doc_dt_update;
extern const char* doc_dt_repeat;

extern const char* doc_math_atan2;
extern const char* doc_math_copysign;
extern const char* doc_math_fmod;
extern const char* doc_math_hypot;
extern const char* doc_math_isclose;
extern const char* doc_math_ldexp;
extern const char* doc_math_logaddexp2;
extern const char* doc_math_logaddexp;
extern const char* doc_math_pow;
extern const char* doc_math_round;

extern const char* doc_models_aggregate;

extern const char* doc_models_Ftrl;
extern const char* doc_models_Ftrl___init__;
extern const char* doc_models_Ftrl_alpha;
extern const char* doc_models_Ftrl_beta;
extern const char* doc_models_Ftrl_colnames;
extern const char* doc_models_Ftrl_colname_hashes;
extern const char* doc_models_Ftrl_double_precision;
extern const char* doc_models_Ftrl_feature_importances;
extern const char* doc_models_Ftrl_fit;
extern const char* doc_models_Ftrl_interactions;
extern const char* doc_models_Ftrl_labels;
extern const char* doc_models_Ftrl_lambda1;
extern const char* doc_models_Ftrl_lambda2;
extern const char* doc_models_Ftrl_model;
extern const char* doc_models_Ftrl_model_type;
extern const char* doc_models_Ftrl_model_type_trained;
extern const char* doc_models_Ftrl_mantissa_nbits;
extern const char* doc_models_Ftrl_nbins;
extern const char* doc_models_Ftrl_negative_class;
extern const char* doc_models_Ftrl_nepochs;
extern const char* doc_models_Ftrl_params;
extern const char* doc_models_Ftrl_predict;
extern const char* doc_models_Ftrl_reset;

extern const char* doc_models_LinearModel;
extern const char* doc_models_LinearModel___init__;
extern const char* doc_models_LinearModel_eta0;
extern const char* doc_models_LinearModel_eta_decay;
extern const char* doc_models_LinearModel_eta_drop_rate;
extern const char* doc_models_LinearModel_eta_schedule;
extern const char* doc_models_LinearModel_double_precision;
extern const char* doc_models_LinearModel_fit;
extern const char* doc_models_LinearModel_is_fitted;
extern const char* doc_models_LinearModel_labels;
extern const char* doc_models_LinearModel_lambda1;
extern const char* doc_models_LinearModel_lambda2;
extern const char* doc_models_LinearModel_model;
extern const char* doc_models_LinearModel_model_type;
extern const char* doc_models_LinearModel_negative_class;
extern const char* doc_models_LinearModel_nepochs;
extern const char* doc_models_LinearModel_params;
extern const char* doc_models_LinearModel_predict;
extern const char* doc_models_LinearModel_reset;
extern const char* doc_models_LinearModel_seed;

extern const char* doc_re_match;

extern const char* doc_str_len;
extern const char* doc_str_slice;
extern const char* doc_str_split_into_nhot;

extern const char* doc_time_day;
extern const char* doc_time_day_of_week;
extern const char* doc_time_hour;
extern const char* doc_time_minute;
extern const char* doc_time_month;
extern const char* doc_time_nanosecond;
extern const char* doc_time_second;
extern const char* doc_time_year;
extern const char* doc_time_ymd;
extern const char* doc_time_ymdt;

extern const char* doc_Frame;
extern const char* doc_Frame___init__;
extern const char* doc_Frame___sizeof__;
extern const char* doc_Frame_cbind;
extern const char* doc_Frame_colindex;
extern const char* doc_Frame_copy;
extern const char* doc_Frame_countna1;
extern const char* doc_Frame_countna;
extern const char* doc_Frame_export_names;
extern const char* doc_Frame_head;
extern const char* doc_Frame_key;
extern const char* doc_Frame_kurt1;
extern const char* doc_Frame_kurt;
extern const char* doc_Frame_ltypes;
extern const char* doc_Frame_materialize;
extern const char* doc_Frame_max1;
extern const char* doc_Frame_max;
extern const char* doc_Frame_mean1;
extern const char* doc_Frame_mean;
extern const char* doc_Frame_meta;
extern const char* doc_Frame_min1;
extern const char* doc_Frame_min;
extern const char* doc_Frame_mode1;
extern const char* doc_Frame_mode;
extern const char* doc_Frame_names;
extern const char* doc_Frame_ncols;
extern const char* doc_Frame_nmodal1;
extern const char* doc_Frame_nmodal;
extern const char* doc_Frame_nrows;
extern const char* doc_Frame_nunique1;
extern const char* doc_Frame_nunique;
extern const char* doc_Frame_rbind;
extern const char* doc_Frame_replace;
extern const char* doc_Frame_sd1;
extern const char* doc_Frame_sd;
extern const char* doc_Frame_shape;
extern const char* doc_Frame_skew1;
extern const char* doc_Frame_skew;
extern const char* doc_Frame_sort;
extern const char* doc_Frame_source;
extern const char* doc_Frame_stype;
extern const char* doc_Frame_stypes;
extern const char* doc_Frame_sum1;
extern const char* doc_Frame_sum;
extern const char* doc_Frame_tail;
extern const char* doc_Frame_to_arrow;
extern const char* doc_Frame_to_csv;
extern const char* doc_Frame_to_dict;
extern const char* doc_Frame_to_jay;
extern const char* doc_Frame_to_list;
extern const char* doc_Frame_to_numpy;
extern const char* doc_Frame_to_pandas;
extern const char* doc_Frame_to_tuples;
extern const char* doc_Frame_type;
extern const char* doc_Frame_types;
extern const char* doc_Frame_view;

extern const char* doc_Namespace;

extern const char* doc_Type;
extern const char* doc_Type_arr32;
extern const char* doc_Type_arr64;
extern const char* doc_Type_max;
extern const char* doc_Type_min;
extern const char* doc_Type_name;


}  // namespace dt
#endif
