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
#include "column/func_nary.h"
#include "column/latent.h"
#include "column/isna.h"
#include "column/nth.h"
#include "documentation.h"
#include "expr/fnary/fnary.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "parallel/api.h"
#include "python/xargs.h"
namespace dt {
namespace expr {


template<size_t SKIPNA>
class FExpr_Nth : public FExpr_Func {
  private:
    ptrExpr arg_;
    int32_t n_;

  public:
    FExpr_Nth(ptrExpr&& arg, py::oobj n)
      : arg_(std::move(arg))
       {n_ = n.to_int32_strict();}

    std::string repr() const override {
      std::string out = "nth";
      out += '(';
      out += arg_->repr();
      out += ", n=";
      out += std::to_string(n_);
      if (SKIPNA == 0) {
        out += ", skipna=None";
      } else if (SKIPNA == 1) {
          out += ", skipna=any";
        } else if (SKIPNA == 2) {
            out += ", skipna=all";
          }      
      out += ')';
      return out;
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

    static bool op_rowany(size_t i, int8_t* out, const colvec& columns) {
      for (const auto& col : columns) {
        int8_t x;
        bool xvalid = col.get_element(i, &x);
        if (xvalid && x) {
          *out = 1;
          return true;
        }
      }
      *out = 0;
      return true;
    }

    static bool op_rowall(size_t i, int8_t* out, const colvec& columns) {
      for (const auto& col : columns) {
        int8_t x;
        bool xvalid = col.get_element(i, &x);
        if (!xvalid || x == 0) {
          *out = 0;
          return true;
        }
      }
      *out = 1;
      return true;
    }

    static Column make_bool_column(colvec&& columns, const size_t nrows, const size_t ncols) {
      if (SKIPNA == 1) {
        return Column(new FuncNary_ColumnImpl<int8_t>(
                      std::move(columns), op_rowany, nrows, SType::BOOL));
      }
      return Column(new FuncNary_ColumnImpl<int8_t>(
                    std::move(columns), op_rowall, nrows, SType::BOOL));
      
    } 

    template <bool POSITIVE>
    static RowIndex rowindex_nth(Column& col, const Groupby& gby) {
      Buffer buf = Buffer::mem(col.nrows() * sizeof(int32_t));
      auto indices = static_cast<int32_t*>(buf.xptr());
      Latent_ColumnImpl::vivify(col);

      dt::parallel_for_dynamic(
        gby.size(),
        [&](size_t gi) {
          size_t i1, i2;
          gby.get_group(gi, &i1, &i2);
          size_t n = POSITIVE? i1: i2 - 1;
          int8_t value;
          bool is_valid;       

          if (POSITIVE) {
            for (size_t i = i1; i < i2; ++i) {
              is_valid = col.get_element(i, &value);
              if (value==0 && is_valid) {
                indices[n] = static_cast<int32_t>(i);
                n += 1;
              }
            }
            for (size_t j = n; j < i2; ++j){
              indices[j] = RowIndex::NA<int32_t>;
            }    
          } else {
              for (size_t i = i2; i-- > i1;) {              
                is_valid = col.get_element(i, &value);
                if (value==0 && is_valid) {
                  indices[n] = static_cast<int32_t>(i);
                  n -= 1;
                }
              }
              for (size_t j = n+1; j-- > i1;){
                indices[j] = RowIndex::NA<int32_t>;
              }    
            }                   
        } 
      );

      return RowIndex(std::move(buf), RowIndex::ARR32|RowIndex::SORTED);
    }

    Workframe evaluate_n(EvalContext &ctx) const override {
      Workframe wf = arg_->evaluate_n(ctx);
      Workframe outputs(ctx);
      Groupby gby = ctx.get_groupby();

      // Check if the input frame is grouped as `GtoONE`
      bool is_wf_grouped = (wf.get_grouping_mode() == Grouping::GtoONE);

      if (is_wf_grouped) {
        // Check if the input frame columns are grouped
        bool are_cols_grouped = ctx.has_group_column(
                                  wf.get_frame_id(0),
                                  wf.get_column_id(0)
                                );

        if (!are_cols_grouped) {
          // When the input frame is `GtoONE`, but columns are not grouped,
          // it means we are dealing with the output of another reducer.
          // In such a case we create a new groupby, that has one element
          // per a group. This may not be optimal performance-wise,
          // but chained reducers is a very rare scenario.
          xassert(gby.size() == wf.nrows());
          gby = Groupby::nrows_groups(gby.size());
        }
      }

      if (SKIPNA == 0) {
        for (size_t i = 0; i < wf.ncols(); ++i) {
          bool is_grouped = ctx.has_group_column(
                              wf.get_frame_id(i),
                              wf.get_column_id(i)
                            );
          Column coli = evaluate1(wf.retrieve_column(i), gby, is_grouped, n_);        
          outputs.add_column(std::move(coli), wf.retrieve_name(i), Grouping::GtoONE); 
        }
      } else {
          Workframe wf_skipna = arg_->evaluate_n(ctx);
          colvec columns;
          size_t ncols = wf_skipna.ncols();
          size_t nrows = wf_skipna.nrows();
          columns.reserve(ncols);
          for (size_t i = 0; i < ncols; ++i) {
            Column coli = make_isna_col(wf_skipna.retrieve_column(i));
            columns.push_back(std::move(coli));
          }
          Column bool_column = make_bool_column(std::move(columns), nrows, ncols);
          RowIndex ri = n_ < 0 ? rowindex_nth<false>(bool_column, gby)
                               : rowindex_nth<true>(bool_column, gby);
          for (size_t i = 0; i < wf.ncols(); ++i) {
            bool is_grouped = ctx.has_group_column(
                                wf.get_frame_id(i),
                                wf.get_column_id(i)
                              );
            Column coli = wf.retrieve_column(i);
            coli.apply_rowindex(ri);
            coli = evaluate1(std::move(coli), gby, is_grouped, n_);        
            outputs.add_column(std::move(coli), wf.retrieve_name(i), Grouping::GtoONE); 
          }
        }
            
      return outputs;
    }   


    Column evaluate1(Column&& col, const Groupby& gby, bool is_grouped, const int32_t n) const {
      SType stype = col.stype();
      switch (stype) {
        case SType::VOID:    return Column(new ConstNa_ColumnImpl(gby.size()));
        case SType::BOOL:
        case SType::INT8:    return make<int8_t>(std::move(col), gby, is_grouped, n);
        case SType::INT16:   return make<int16_t>(std::move(col), gby, is_grouped, n);
        case SType::DATE32:
        case SType::INT32:   return make<int32_t>(std::move(col), gby, is_grouped, n);
        case SType::TIME64:
        case SType::INT64:   return make<int64_t>(std::move(col), gby, is_grouped, n);
        case SType::FLOAT32: return make<float>(std::move(col), gby, is_grouped, n);
        case SType::FLOAT64: return make<double>(std::move(col), gby, is_grouped, n);
        case SType::STR32:   return make<CString>(std::move(col), gby, is_grouped, n);
        case SType::STR64:   return make<CString>(std::move(col), gby, is_grouped,n);
        default:
          throw TypeError()
            << "Invalid column of type `" << stype << "` in " << repr();
      }
    }


    template <typename T>
    Column make(Column&& col, const Groupby& gby, bool is_grouped, int32_t n) const {
      return Column(new Nth_ColumnImpl<T>(std::move(col), gby, is_grouped, n));
    }

};


static py::oobj pyfn_nth(const py::XArgs& args) {
  auto arg = args[0].to_oobj();
  auto n = args[1].to<py::oobj>(py::oint(0));
  auto skipna = args[2].to_oobj_or_none();
  if (!skipna.is_none()) {
    if (!skipna.is_string()) {
      throw TypeError() << "The argument for the `skipna` parameter "
                  <<"in function datatable.nth() should either be None, "
                  <<"or a string, instead got "<<skipna.typeobj();

    }
    std::string skip_na = skipna.to_string();
    if (skip_na != "any" && skip_na != "all") {
      throw TypeError() << "The argument for the `skipna` parameter "
                <<"in function datatable.nth() should either be None, "
                <<"any or all, instead got "<<skipna.repr();
    }
  }
  if (!n.is_int()) {
    throw TypeError() << "The argument for the `nth` parameter "
                <<"in function datatable.nth() should be an integer, "
                <<"instead got "<<n.typeobj();
  }
  if (!skipna.is_none()) {
    std::string skip_na = skipna.to_string();
    if (skip_na == "any") {
      return PyFExpr::make(new FExpr_Nth<1>(as_fexpr(arg), n));
    }
    if (skip_na == "all") {
      return PyFExpr::make(new FExpr_Nth<2>(as_fexpr(arg), n));
    }
    
  } 
  return PyFExpr::make(new FExpr_Nth<0>(as_fexpr(arg), n));  
}


DECLARE_PYFN(&pyfn_nth)
    ->name("nth")
    ->docs(doc_dt_nth)
    ->arg_names({"cols", "n", "skipna"})
    ->n_positional_args(1)
    ->n_positional_or_keyword_args(2)
    ->n_required_args(1);


}}  // dt::expr
