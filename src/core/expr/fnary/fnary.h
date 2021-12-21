//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#ifndef dt_EXPR_FNARY_FNARY_h
#define dt_EXPR_FNARY_FNARY_h
#include <memory>
#include "expr/declarations.h"
#include "expr/fexpr_func.h"
#include "expr/op.h"
#include "python/args.h"
namespace dt {
namespace expr {


enum RowFnKind : int {
  FN_ROWALL,
  FN_ROWANY,
  FN_ROWARGMAX,
  FN_ROWARGMIN,
  FN_ROWCOUNT,
  FN_ROWFIRST,
  FN_ROWLAST,
  FN_ROWMAX,
  FN_ROWMEAN,
  FN_ROWMIN,
  FN_ROWSD,
  FN_ROWSUM,
};

py::oobj py_rowfn(const py::XArgs& args);


class FExpr_RowFn : public FExpr_Func {
  private:
    ptrExpr args_;

  public:
    FExpr_RowFn(ptrExpr&& args);
    std::string repr() const override;
    Workframe evaluate_n(EvalContext& ctx) const override;

    virtual std::string name() const = 0;
    virtual Column apply_function(std::vector<Column>&& columns) const = 0;

    SType common_numeric_stype(const colvec&) const;
    void promote_columns(colvec& columns, SType target_stype) const;
};



class FExpr_RowAll : public FExpr_RowFn {
  public:
    using FExpr_RowFn::FExpr_RowFn;

    std::string name() const override;
    Column apply_function(std::vector<Column>&& columns) const override;
};



class FExpr_RowAny : public FExpr_RowFn {
  public:
    using FExpr_RowFn::FExpr_RowFn;

    std::string name() const override;
    Column apply_function(std::vector<Column>&& columns) const override;
};



class FExpr_RowCount : public FExpr_RowFn {
  public:
    using FExpr_RowFn::FExpr_RowFn;

    std::string name() const override;
    Column apply_function(std::vector<Column>&& columns) const override;
};



template <bool FIRST>
class FExpr_RowFirstLast : public FExpr_RowFn {
  public:
    using FExpr_RowFn::FExpr_RowFn;

    std::string name() const override;
    Column apply_function(std::vector<Column>&& columns) const override;
};

extern template class FExpr_RowFirstLast<true>;
extern template class FExpr_RowFirstLast<false>;



template <bool MIN, bool ARG=false>
class FExpr_RowMinMax : public FExpr_RowFn {
  public:
    using FExpr_RowFn::FExpr_RowFn;

    std::string name() const override;
    Column apply_function(std::vector<Column>&& columns) const override;
};

extern template class FExpr_RowMinMax<true,true>;
extern template class FExpr_RowMinMax<false,true>;
extern template class FExpr_RowMinMax<true,false>;
extern template class FExpr_RowMinMax<false,false>;



class FExpr_RowMean : public FExpr_RowFn {
  public:
    using FExpr_RowFn::FExpr_RowFn;

    std::string name() const override;
    Column apply_function(std::vector<Column>&& columns) const override;
};



class FExpr_RowSd : public FExpr_RowFn {
  public:
    using FExpr_RowFn::FExpr_RowFn;

    std::string name() const override;
    Column apply_function(std::vector<Column>&& columns) const override;
};



class FExpr_RowSum : public FExpr_RowFn {
  public:
    using FExpr_RowFn::FExpr_RowFn;

    std::string name() const override;
    Column apply_function(std::vector<Column>&& columns) const override;
};




}}  // namespace dt::expr
#endif
