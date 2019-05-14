//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <cmath>
#include "expr/expr_unaryop.h"
#include "parallel/api.h"
#include "types.h"
namespace dt {
namespace expr {

// Singleton instance
unary_infos unary_library;



//------------------------------------------------------------------------------
// Mapper functions
//------------------------------------------------------------------------------

template<typename IT, typename OT, OT(*OP)(IT)>
void map11(const Column* inputcol, Column* outputcol) {
  xassert(inputcol->nrows == outputcol->nrows);
  size_t nrows = inputcol->nrows;
  auto inp = static_cast<const IT*>(inputcol->data());
  auto out = static_cast<OT*>(outputcol->data_w());
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = OP(inp[i]);
    });
}


template<typename IT, typename OT>
void map_str_len(const Column* inputcol, Column* outputcol) {
  size_t nrows = inputcol->nrows;
  auto inp = dynamic_cast<const StringColumn<IT>*>(inputcol)->offsets();
  auto out = static_cast<OT*>(outputcol->data_w());
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = ISNA<IT>(inp[i])
                  ? GETNA<OT>()
                  : static_cast<OT>((inp[i] - inp[i-1]) & ~GETNA<IT>());
    });
}


template<typename T>
void map_str_isna(const Column* inputcol, Column* outputcol) {
  size_t nrows = inputcol->nrows;
  const T* inp = dynamic_cast<const StringColumn<T>*>(inputcol)->offsets();
  auto out = static_cast<int8_t*>(outputcol->data_w());
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = ISNA<T>(inp[i]);
    });
}


template <int8_t VAL>
void set_const(const Column*, Column* outputcol) {
  xassert(outputcol->stype() == SType::BOOL);
  size_t nrows = outputcol->nrows;
  auto out = static_cast<int8_t*>(outputcol->data_w());
  dt::parallel_for_static(nrows, [=](size_t i){ out[i] = VAL; });
}




//------------------------------------------------------------------------------
// Operator implementations
//------------------------------------------------------------------------------

// If `x` is integer NA, then (-(INT_MIN)) == INT_MIN due to overflow.
// If `x` is floating-point NA, then (-NaN) == NaN by rules of IEEE754.
// Thus, in both cases (-NA)==NA, as desired.
template <typename T>
inline static T op_minus(T x) {
  return -x;
}

template <typename T>
inline static bool op_isna(T x) {
  return ISNA<T>(x);
}


template <typename T>
inline static T op_abs(T x) {
  // If T is floating point and x is NA, then (x < 0) will evaluate to false;
  // If T is integer and x is NA, then (x < 0) will be true, but -x will be
  // equal to x.
  // Thus, in all cases we'll have `abs(NA) == NA`.
  return (x < 0) ? -x : x;
}


inline static int8_t op_invert_bool(int8_t x) {
  return ISNA<int8_t>(x)? x : !x;
}

template <typename T>
inline static T op_inverse(T x) {
  return ISNA<T>(x)? x : ~x;
}




//------------------------------------------------------------------------------
// expr_unaryop
//------------------------------------------------------------------------------

expr_unaryop::expr_unaryop(pexpr&& a, Op op)
  : arg(std::move(a)), opcode(op) {}


bool expr_unaryop::is_negated_expr() const {
  return opcode == Op::UMINUS;
}

pexpr expr_unaryop::get_negated_expr() {
  pexpr res;
  if (opcode == Op::UMINUS) {
    res = std::move(arg);
  }
  return res;
}


SType expr_unaryop::resolve(const workframe& wf) {
  SType input_stype = arg->resolve(wf);
  return unary_library.xget(opcode, input_stype).output_stype;
}


GroupbyMode expr_unaryop::get_groupby_mode(const workframe& wf) const {
  return arg->get_groupby_mode(wf);
}


colptr expr_unaryop::evaluate_eager(workframe& wf) {
  auto input_column = arg->evaluate_eager(wf);

  // Already checked existence of key in `resolve()`
  SType input_stype = input_column->stype();
  const auto& ui = unary_library.xget(opcode, input_stype);
  SType output_stype = ui.output_stype;

  if (ui.fn == nullptr) {
    return colptr(input_column->shallowcopy());
  }
  input_column->materialize();

  auto output_column = Column::new_data_column(output_stype, input_column->nrows);
  ui.fn(input_column.get(), output_column);

  return colptr(output_column);
}




//------------------------------------------------------------------------------
// one-time initialization
//------------------------------------------------------------------------------

unary_infos::unary_infos() {
  constexpr SType bool8 = SType::BOOL;
  constexpr SType int8  = SType::INT8;
  constexpr SType int16 = SType::INT16;
  constexpr SType int32 = SType::INT32;
  constexpr SType int64 = SType::INT64;
  constexpr SType flt32 = SType::FLOAT32;
  constexpr SType flt64 = SType::FLOAT64;
  constexpr SType str32 = SType::STR32;
  constexpr SType str64 = SType::STR64;

  add(Op::UPLUS, bool8, int8,  nullptr);
  add(Op::UPLUS, int8, int8,  nullptr);
  add(Op::UPLUS, int16, int16, nullptr);
  add(Op::UPLUS, int32, int32, nullptr);
  add(Op::UPLUS, int64, int64, nullptr);
  add(Op::UPLUS, flt32, flt32, nullptr);
  add(Op::UPLUS, flt64, flt64, nullptr);

  add(Op::UMINUS, bool8, int8,  map11<int8_t,  int8_t,  op_minus<int8_t>>);
  add(Op::UMINUS, int8,  int8,  map11<int8_t,  int8_t,  op_minus<int8_t>>);
  add(Op::UMINUS, int16, int16, map11<int16_t, int16_t, op_minus<int16_t>>);
  add(Op::UMINUS, int32, int32, map11<int32_t, int32_t, op_minus<int32_t>>);
  add(Op::UMINUS, int64, int64, map11<int64_t, int64_t, op_minus<int64_t>>);
  add(Op::UMINUS, flt32, flt32, map11<float,   float,   op_minus<float>>);
  add(Op::UMINUS, flt64, flt64, map11<double,  double,  op_minus<double>>);

  add(Op::UINVERT, bool8, bool8, map11<int8_t,  int8_t,  op_invert_bool>);
  add(Op::UINVERT, int8,  int8,  map11<int8_t,  int8_t,  op_inverse<int8_t>>);
  add(Op::UINVERT, int16, int16, map11<int16_t, int16_t, op_inverse<int16_t>>);
  add(Op::UINVERT, int32, int32, map11<int32_t, int32_t, op_inverse<int32_t>>);
  add(Op::UINVERT, int64, int64, map11<int64_t, int64_t, op_inverse<int64_t>>);

  add(Op::ISNA, bool8, bool8, map11<int8_t,   bool, op_isna<int8_t>>);
  add(Op::ISNA, int8,  bool8, map11<int8_t,   bool, op_isna<int8_t>>);
  add(Op::ISNA, int16, bool8, map11<int16_t,  bool, op_isna<int16_t>>);
  add(Op::ISNA, int32, bool8, map11<int32_t,  bool, op_isna<int32_t>>);
  add(Op::ISNA, int64, bool8, map11<int64_t,  bool, op_isna<int64_t>>);
  add(Op::ISNA, flt32, bool8, map11<float,    bool, std::isnan>);
  add(Op::ISNA, flt64, bool8, map11<double,   bool, std::isnan>);
  add(Op::ISNA, str32, bool8, map_str_isna<uint32_t>);
  add(Op::ISNA, str64, bool8, map_str_isna<uint64_t>);

  add(Op::ISFINITE, bool8, bool8, set_const<1>);
  add(Op::ISFINITE, int8,  bool8, set_const<1>);
  add(Op::ISFINITE, int16, bool8, set_const<1>);
  add(Op::ISFINITE, int32, bool8, set_const<1>);
  add(Op::ISFINITE, int64, bool8, set_const<1>);
  add(Op::ISFINITE, flt32, bool8, map11<float,  bool, std::isfinite>);
  add(Op::ISFINITE, flt64, bool8, map11<double, bool, std::isfinite>);

  add(Op::ISINF, bool8, bool8, set_const<0>);
  add(Op::ISINF, int8,  bool8, set_const<0>);
  add(Op::ISINF, int16, bool8, set_const<0>);
  add(Op::ISINF, int32, bool8, set_const<0>);
  add(Op::ISINF, int64, bool8, set_const<0>);
  add(Op::ISINF, flt32, bool8, map11<float,  bool, std::isinf>);
  add(Op::ISINF, flt64, bool8, map11<double, bool, std::isinf>);

  add(Op::ABS, bool8, int8, nullptr);
  add(Op::ABS, int8,  int8,  map11<int8_t,  int8_t,  op_abs<int8_t>>);
  add(Op::ABS, int16, int16, map11<int16_t, int16_t, op_abs<int16_t>>);
  add(Op::ABS, int32, int32, map11<int32_t, int32_t, op_abs<int32_t>>);
  add(Op::ABS, int64, int64, map11<int64_t, int64_t, op_abs<int64_t>>);
  add(Op::ABS, flt32, flt32, map11<float,   float,   std::abs>);
  add(Op::ABS, flt64, flt64, map11<double,  double,  std::abs>);

  add(Op::CEIL, bool8, int8,  nullptr);
  add(Op::CEIL, int8,  int8,  nullptr);
  add(Op::CEIL, int16, int16, nullptr);
  add(Op::CEIL, int32, int32, nullptr);
  add(Op::CEIL, int64, int64, nullptr);
  add(Op::CEIL, flt32, flt32, map11<float,  float,  std::ceil>);
  add(Op::CEIL, flt64, flt64, map11<double, double, std::ceil>);

  add(Op::FLOOR, bool8, int8,  nullptr);
  add(Op::FLOOR, int8,  int8,  nullptr);
  add(Op::FLOOR, int16, int16, nullptr);
  add(Op::FLOOR, int32, int32, nullptr);
  add(Op::FLOOR, int64, int64, nullptr);
  add(Op::FLOOR, flt32, flt32, map11<float,  float,  std::floor>);
  add(Op::FLOOR, flt64, flt64, map11<double, double, std::floor>);

  add(Op::TRUNC, bool8, int8,  nullptr);
  add(Op::TRUNC, int8,  int8,  nullptr);
  add(Op::TRUNC, int16, int16, nullptr);
  add(Op::TRUNC, int32, int32, nullptr);
  add(Op::TRUNC, int64, int64, nullptr);
  add(Op::TRUNC, flt32, flt32, map11<float,  float,  std::trunc>);
  add(Op::TRUNC, flt64, flt64, map11<double, double, std::trunc>);

  add(Op::LEN, str32, int32, map_str_len<uint32_t, int32_t>);
  add(Op::LEN, str64, int64, map_str_len<uint64_t, int64_t>);

  set_name(Op::UPLUS, "+");
  set_name(Op::UMINUS, "-");
  set_name(Op::UINVERT, "~");
  set_name(Op::ISNA, "isna");
  set_name(Op::ISFINITE, "isfinite");
  set_name(Op::ISINF, "isinf");
  set_name(Op::ABS, "abs");
  set_name(Op::CEIL, "ceil");
  set_name(Op::FLOOR, "floor");
  set_name(Op::TRUNC, "trunc");
  set_name(Op::LEN, "len");
}


constexpr size_t unary_infos::id(Op op) noexcept {
  return static_cast<size_t>(op) - UNOP_FIRST;
}

constexpr size_t unary_infos::id(Op op, SType stype) noexcept {
  return static_cast<size_t>(op) * DT_STYPES_COUNT +
         static_cast<size_t>(stype);
}

void unary_infos::set_name(Op op, const std::string& name) {
  names[id(op)] = name;
}

void unary_infos::add(Op op, SType input_stype, SType output_stype,
                      unary_func_t fn)
{
  size_t entry_id = id(op, input_stype);
  xassert(info.count(entry_id) == 0);
  info[entry_id] = {fn, output_stype};
}


const unary_infos::uinfo& unary_infos::xget(Op op, SType input_stype) const {
  size_t entry_id = id(op, input_stype);
  if (info.count(entry_id)) {
    return info.at(entry_id);
  }
  size_t name_id = id(op);
  const std::string& opname = names.count(name_id)? names.at(name_id) : "";
  auto err = TypeError();
  err << "Cannot apply ";
  if (op == Op::UPLUS || op == Op::UMINUS || op == Op::UINVERT) {
    err << "unary `operator " << opname << "`";
  } else {
    err << "function `" << opname << "()`";
  }
  err << " to a column with stype `" << input_stype << "`";
  throw err;
}



}}  // namespace dt::expr
