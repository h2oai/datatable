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
extern const char* doc_dt_nunique;
extern const char* doc_dt_qcut;
extern const char* doc_dt_rbind;
extern const char* doc_dt_repeat;
extern const char* doc_dt_rowall;
extern const char* doc_dt_rowany;
extern const char* doc_dt_rowargmax;
extern const char* doc_dt_rowargmin;
extern const char* doc_dt_rowcount;
extern const char* doc_dt_rowfirst;
extern const char* doc_dt_rowlast;
extern const char* doc_dt_rowmax;
extern const char* doc_dt_rowmean;
extern const char* doc_dt_rowmin;
extern const char* doc_dt_rowsd;
extern const char* doc_dt_rowsum;
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
extern const char* doc_dt_prod;

extern const char* doc_internal_frame_column_data_r;
extern const char* doc_internal_frame_columns_virtual;
extern const char* doc_internal_frame_integrity_check;
extern const char* doc_internal_get_thread_ids;

extern const char* doc_math_abs;
extern const char* doc_math_arccos;
extern const char* doc_math_arcosh;
extern const char* doc_math_arcsin;
extern const char* doc_math_arctan;
extern const char* doc_math_arsinh;
extern const char* doc_math_artanh;
extern const char* doc_math_atan2;
extern const char* doc_math_cbrt;
extern const char* doc_math_ceil;
extern const char* doc_math_copysign;
extern const char* doc_math_cos;
extern const char* doc_math_cosh;
extern const char* doc_math_deg2rad;
extern const char* doc_math_erf;
extern const char* doc_math_erfc;
extern const char* doc_math_exp2;
extern const char* doc_math_exp;
extern const char* doc_math_expm1;
extern const char* doc_math_fabs;
extern const char* doc_math_floor;
extern const char* doc_math_fmod;
extern const char* doc_math_gamma;
extern const char* doc_math_hypot;
extern const char* doc_math_isclose;
extern const char* doc_math_isfinite;
extern const char* doc_math_isinf;
extern const char* doc_math_isna;
extern const char* doc_math_ldexp;
extern const char* doc_math_lgamma;
extern const char* doc_math_log10;
extern const char* doc_math_log1p;
extern const char* doc_math_log2;
extern const char* doc_math_log;
extern const char* doc_math_logaddexp2;
extern const char* doc_math_logaddexp;
extern const char* doc_math_pow;
extern const char* doc_math_rad2deg;
extern const char* doc_math_rint;
extern const char* doc_math_round;
extern const char* doc_math_sign;
extern const char* doc_math_signbit;
extern const char* doc_math_sin;
extern const char* doc_math_sinh;
extern const char* doc_math_sqrt;
extern const char* doc_math_square;
extern const char* doc_math_tan;
extern const char* doc_math_tanh;
extern const char* doc_math_trunc;

extern const char* doc_models_aggregate;
extern const char* doc_models_kfold;
extern const char* doc_models_kfold_random;

extern const char* doc_models_Ftrl;
extern const char* doc_models_Ftrl___init__;
extern const char* doc_models_Ftrl_alpha;
extern const char* doc_models_Ftrl_beta;
extern const char* doc_models_Ftrl_colname_hashes;
extern const char* doc_models_Ftrl_colnames;
extern const char* doc_models_Ftrl_double_precision;
extern const char* doc_models_Ftrl_feature_importances;
extern const char* doc_models_Ftrl_fit;
extern const char* doc_models_Ftrl_interactions;
extern const char* doc_models_Ftrl_labels;
extern const char* doc_models_Ftrl_lambda1;
extern const char* doc_models_Ftrl_lambda2;
extern const char* doc_models_Ftrl_mantissa_nbits;
extern const char* doc_models_Ftrl_model;
extern const char* doc_models_Ftrl_model_type;
extern const char* doc_models_Ftrl_model_type_trained;
extern const char* doc_models_Ftrl_nbins;
extern const char* doc_models_Ftrl_negative_class;
extern const char* doc_models_Ftrl_nepochs;
extern const char* doc_models_Ftrl_params;
extern const char* doc_models_Ftrl_predict;
extern const char* doc_models_Ftrl_reset;

extern const char* doc_models_LinearModel;
extern const char* doc_models_LinearModel___init__;
extern const char* doc_models_LinearModel_double_precision;
extern const char* doc_models_LinearModel_eta0;
extern const char* doc_models_LinearModel_eta_decay;
extern const char* doc_models_LinearModel_eta_drop_rate;
extern const char* doc_models_LinearModel_eta_schedule;
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

extern const char* doc_options_debug_arg_max_size;
extern const char* doc_options_debug_enabled;
extern const char* doc_options_debug_logger;
extern const char* doc_options_debug_report_args;
extern const char* doc_options_display_allow_unicode;
extern const char* doc_options_display_head_nrows;
extern const char* doc_options_display_interactive;
extern const char* doc_options_display_max_column_width;
extern const char* doc_options_display_max_nrows;
extern const char* doc_options_display_tail_nrows;
extern const char* doc_options_display_use_colors;
extern const char* doc_options_frame_names_auto_index;
extern const char* doc_options_frame_names_auto_prefix;
extern const char* doc_options_fread_log_anonymize;
extern const char* doc_options_fread_log_escape_unicode;
extern const char* doc_options_fread_parse_dates;
extern const char* doc_options_fread_parse_times;
extern const char* doc_options_nthreads;
extern const char* doc_options_progress_allow_interruption;
extern const char* doc_options_progress_callback;
extern const char* doc_options_progress_clear_on_success;
extern const char* doc_options_progress_enabled;
extern const char* doc_options_progress_min_duration;
extern const char* doc_options_progress_updates_per_second;

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

extern const char* doc_FExpr;
extern const char* doc_FExpr_as_type;
extern const char* doc_FExpr_count;
extern const char* doc_FExpr_countna;
extern const char* doc_FExpr_extend;
extern const char* doc_FExpr_first;
extern const char* doc_FExpr_last;
extern const char* doc_FExpr_max;
extern const char* doc_FExpr_mean;
extern const char* doc_FExpr_median;
extern const char* doc_FExpr_min;
extern const char* doc_FExpr_nunique;
extern const char* doc_FExpr_prod;
extern const char* doc_FExpr_remove;
extern const char* doc_FExpr_rowall;
extern const char* doc_FExpr_rowany;
extern const char* doc_FExpr_rowcount;
extern const char* doc_FExpr_rowfirst;
extern const char* doc_FExpr_rowlast;
extern const char* doc_FExpr_rowargmax;
extern const char* doc_FExpr_rowargmin;
extern const char* doc_FExpr_rowmax;
extern const char* doc_FExpr_rowmean;
extern const char* doc_FExpr_rowmin;
extern const char* doc_FExpr_rowsd;
extern const char* doc_FExpr_rowsum;
extern const char* doc_FExpr_sd;
extern const char* doc_FExpr_shift;
extern const char* doc_FExpr_sum;


extern const char* doc_Namespace;

extern const char* doc_Type;
extern const char* doc_Type_arr32;
extern const char* doc_Type_arr64;
extern const char* doc_Type_cat8;
extern const char* doc_Type_cat16;
extern const char* doc_Type_cat32;

extern const char* doc_Type_is_array;
extern const char* doc_Type_is_boolean;
extern const char* doc_Type_is_categorical;
extern const char* doc_Type_is_compound;
extern const char* doc_Type_is_float;
extern const char* doc_Type_is_integer;
extern const char* doc_Type_is_numeric;
extern const char* doc_Type_is_object;
extern const char* doc_Type_is_string;
extern const char* doc_Type_is_temporal;
extern const char* doc_Type_is_void;

extern const char* doc_Type_max;
extern const char* doc_Type_min;
extern const char* doc_Type_name;




}  // namespace dt
#endif
