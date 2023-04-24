//------------------------------------------------------------------------------
// Copyright 2022 H2O.ai
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
#include "column/const.h"
#include "column/ifelse.h"
#include "column/isna.h"
#include "column/latent.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "python/xargs.h"
#include "parallel/api.h"
namespace dt {
namespace expr {


class FExpr_FillNA : public FExpr_Func {
  private:
    ptrExpr arg_;
    ptrExpr value_;
    bool reverse_;
    size_t : 56;

  public:
    FExpr_FillNA(ptrExpr &&arg, ptrExpr &&value, bool reverse)
        : arg_(std::move(arg)),
          value_(std::move(value)),
          reverse_(reverse)
         {}


    std::string repr() const override {
      std::string out = "fillna";
      out += '(';
      out += arg_->repr();
      if (value_->get_expr_kind() != Kind::None) {
        out +=", value=";
        out += value_->repr();
      } else {
        out += ", reverse=";
        out += reverse_? "True" : "False";
      }
      out += ')';
      return out;
    }


    template <bool REVERSE>
    static RowIndex fill_rowindex(Column& col, const Groupby& gby) {
      switch (col.stype()) {
        case SType::BOOL:
        case SType::INT8:    return fill_rowindex<int8_t, REVERSE>(col, gby);
        case SType::INT16:   return fill_rowindex<int16_t, REVERSE>(col, gby);
        case SType::DATE32:
        case SType::INT32:   return fill_rowindex<int32_t, REVERSE>(col, gby);
        case SType::TIME64:
        case SType::INT64:   return fill_rowindex<int64_t, REVERSE>(col, gby);
        case SType::FLOAT32: return fill_rowindex<float, REVERSE>(col, gby);
        case SType::FLOAT64: return fill_rowindex<double, REVERSE>(col, gby);
        case SType::STR32:
        case SType::STR64:   return fill_rowindex<CString, REVERSE>(col, gby);
        default: throw RuntimeError();
      }
    }


    template <typename T, bool REVERSE>
    static RowIndex fill_rowindex(Column& col, const Groupby& gby) {
      Buffer buf = Buffer::mem(col.nrows() * sizeof(int32_t));
      auto indices = static_cast<int32_t*>(buf.xptr());
      Latent_ColumnImpl::vivify(col);

      dt::parallel_for_dynamic(
        gby.size(),
        [&](size_t gi) {
          size_t i1, i2;
          gby.get_group(gi, &i1, &i2);
          size_t fill_id = REVERSE? i2 - 1 : i1;
          T value;
          bool is_valid;

          if (REVERSE) {
            for (size_t i = i2; i-- > i1;) {
              is_valid = col.get_element(i, &value);
              fill_id = is_valid? i : fill_id;
              indices[i] = static_cast<int32_t>(fill_id);
            }
          } else {
            for (size_t i = i1; i < i2; ++i) {
              is_valid = col.get_element(i, &value);
              fill_id = is_valid? i : fill_id;
              indices[i] = static_cast<int32_t>(fill_id);
            }
          }

        }
      );

      return RowIndex(std::move(buf), RowIndex::ARR32|RowIndex::SORTED);
    }


    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe wf = arg_->evaluate_n(ctx);
      bool value_fill = (value_->get_expr_kind() != Kind::None);

      if (value_fill) {
        // Fill with the `value`
        Workframe wf_value = value_->evaluate_n(ctx);
        if (wf_value.ncols() == 1) wf_value.repeat_column(wf.ncols());

        if (wf_value.ncols() != wf.ncols()) {
          throw ValueError() << "The number of columns in function "
            << "`datatable.fillna()` does not match the number of "
            << "the provided values: "
            << wf.ncols() << " vs " << wf_value.ncols();
        }

        wf.sync_grouping_mode(wf_value);
        Workframe outputs(ctx);

        for (size_t i = 0; i < wf.ncols(); ++i) {
          Column col_orig = Column(wf.get_column(i));
          Column col_cond = make_isna_col(wf.retrieve_column(i));
          Column col_repl = wf_value.retrieve_column(i);

          SType st = common_stype(col_orig.stype(), col_repl.stype());
          col_repl.cast_inplace(st);
          col_orig.cast_inplace(st);

          Column col_out = Column(new IfElse_ColumnImpl(
                             std::move(col_cond),
                             std::move(col_repl),
                             std::move(col_orig)
                           ));
          wf.replace_column(i, std::move(col_out));
        }

      } else {
        // Fill with the previous/subsequent non-missing values
        Groupby gby = ctx.get_groupby();
        wf.increase_grouping_mode(Grouping::GtoALL);

        for (size_t i = 0; i < wf.ncols(); ++i) {
          bool is_grouped = ctx.has_group_column(
                              wf.get_frame_id(i),
                              wf.get_column_id(i)
                            );
          if (is_grouped || wf.get_column(i).stype() == SType::VOID) continue;

          Column coli = wf.retrieve_column(i);
          auto stats = coli.get_stats_if_exist();
          bool na_stats_exists = (stats && stats->is_computed(Stat::NaCount));
          bool has_nas = na_stats_exists? stats->nacount()
                                        : true;

          if (has_nas) {
              RowIndex ri = reverse_? fill_rowindex<true>(coli, gby)
                                    : fill_rowindex<false>(coli, gby);
              coli.apply_rowindex(ri);
          }
          wf.replace_column(i, std::move(coli));
        }
      }

      return wf;
    }


    static Column make_isna_col(Column&& col) {
      switch (col.stype()) {
        case SType::VOID:    return Const_ColumnImpl::make_bool_column(col.nrows(), true);
        case SType::BOOL:
        case SType::INT8:    return Column(new Isna_ColumnImpl<int8_t>(std::move(col)));
        case SType::INT16:   return Column(new Isna_ColumnImpl<int16_t>(std::move(col)));
        case SType::DATE32:
        case SType::INT32:   return Column(new Isna_ColumnImpl<int32_t>(std::move(col)));
        case SType::TIME64:
        case SType::INT64:   return Column(new Isna_ColumnImpl<int64_t>(std::move(col)));
        case SType::FLOAT32: return Column(new Isna_ColumnImpl<float>(std::move(col)));
        case SType::FLOAT64: return Column(new Isna_ColumnImpl<double>(std::move(col)));
        case SType::STR32:
        case SType::STR64:   return Column(new Isna_ColumnImpl<CString>(std::move(col)));
        default: throw RuntimeError();
      }
    }

};


static py::oobj pyfn_fillna(const py::XArgs &args) {
  auto cols = args[0].to_oobj();
  auto value = args[1].to_oobj_or_none();
  auto reverse = args[2].to_oobj_or_none();
  if (!value.is_none() && !reverse.is_none()) {
    throw ValueError() << "Parameters `value` and `reverse` in function "
      << "datatable.fillna() cannot be both set at the same time";
  }

  bool reverse_ = reverse.is_none()? false
                                   : reverse.to_bool_strict();

  return PyFExpr::make(new FExpr_FillNA(
           as_fexpr(cols),
           as_fexpr(value),
           reverse_
         ));
}


DECLARE_PYFN(&pyfn_fillna)
    ->name("fillna")
    ->docs(doc_dt_fillna)
    ->arg_names({"cols", "value", "reverse"})
    ->n_required_args(1)
    ->n_positional_args(1)
    ->n_keyword_args(2);


}}  // dt::expr

