//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "expr/expr_unaryop.h"
#include "types.h"
namespace dt {
namespace expr {

typedef void (*mapperfn)(int64_t row0, int64_t row1, void** params);


static std::vector<std::string> unop_names;
static std::unordered_map<size_t, SType> unop_rules;
static size_t id(Op opcode);
static size_t id(Op opcode, SType st1);



//------------------------------------------------------------------------------
// Final mapper function
//------------------------------------------------------------------------------

template<typename IT, typename OT, OT (*OP)(IT)>
static void map_n(int64_t row0, int64_t row1, void** params) {
  Column* col0 = static_cast<Column*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  const IT* arg_data = static_cast<const IT*>(col0->data());
  OT* res_data = static_cast<OT*>(col1->data_w());
  for (int64_t i = row0; i < row1; ++i) {
    res_data[i] = OP(arg_data[i]);
  }
}

template<typename IT, typename OT, OT (*OP)(IT, IT)>
static void strmap_n(int64_t row0, int64_t row1, void** params) {
  StringColumn<IT>* col0 = static_cast<StringColumn<IT>*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  const IT* arg_data = col0->offsets();
  OT* res_data = static_cast<OT*>(col1->data_w());
  for (int64_t i = row0; i < row1; ++i) {
    res_data[i] = OP(arg_data[i - 1] & ~GETNA<IT>(), arg_data[i]);
  }
}


//------------------------------------------------------------------------------
// Operator implementations
//------------------------------------------------------------------------------

template<typename T>
inline static T op_minus(T x) {
  return IsIntNA<T>(x) ? x : -x;
}

template<typename T>
inline static int8_t op_isna(T x) {
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

template <typename T>
inline static double op_exp(T x) {
  return ISNA<T>(x) ? GETNA<double>() : std::exp(static_cast<double>(x));
}


template <typename T>
inline static double op_loge(T x) {
  return ISNA<T>(x) || x < 0 ? GETNA<double>()
                             : std::log(static_cast<double>(x));
}


template <typename T>
inline static double op_log10(T x) {
  return ISNA<T>(x) || x < 0 ? GETNA<double>()
                             : std::log10(static_cast<double>(x));
}


template<typename T>
struct Inverse {
  inline static T impl(T x) {
    return ISNA<T>(x) ? x : ~x;
  }
};

template<>
struct Inverse<float> {
  inline static float impl(float) { return 0; }
};

template<>
struct Inverse<double> {
  inline static double impl(double) { return 0; }
};

inline static int8_t bool_inverse(int8_t x) {
  return ISNA<int8_t>(x)? x : !x;
}



//------------------------------------------------------------------------------
// String operators
//------------------------------------------------------------------------------

template <typename T>
inline static int8_t op_isna_str(T, T end) {
  return ISNA<T>(end);
}

template <typename IT, typename OT>
inline static OT op_len_str(IT start, IT end) {
  return ISNA<IT>(end)? GETNA<OT>() : static_cast<OT>(end - start);
}



//------------------------------------------------------------------------------
// Method resolution
//------------------------------------------------------------------------------

template<typename IT>
static mapperfn resolve1(Op opcode) {
  switch (opcode) {
    case Op::ISNA:    return map_n<IT, int8_t, op_isna<IT>>;
    case Op::MINUS:   return map_n<IT, IT, op_minus<IT>>;
    case Op::ABS:     return map_n<IT, IT, op_abs<IT>>;
    case Op::EXP:     return map_n<IT, double, op_exp<IT>>;
    case Op::LOGE:    return map_n<IT, double, op_loge<IT>>;
    case Op::LOG10:   return map_n<IT, double, op_log10<IT>>;
    case Op::INVERT:
      if (std::is_floating_point<IT>::value) return nullptr;
      return map_n<IT, IT, Inverse<IT>::impl>;
    default:                return nullptr;
  }
}


template<typename T>
static mapperfn resolve_str(Op opcode) {
  using OT = typename std::make_signed<T>::type;
  switch (opcode) {
    case Op::ISNA: return strmap_n<T, int8_t, op_isna_str<T>>;
    case Op::LEN:  return strmap_n<T, OT, op_len_str<T, OT>>;
    default:             return nullptr;
  }
}


static mapperfn resolve0(SType stype, Op opcode) {
  switch (stype) {
    case SType::BOOL:
      if (opcode == Op::INVERT) return map_n<int8_t, int8_t, bool_inverse>;
      return resolve1<int8_t>(opcode);
    case SType::INT8:    return resolve1<int8_t>(opcode);
    case SType::INT16:   return resolve1<int16_t>(opcode);
    case SType::INT32:   return resolve1<int32_t>(opcode);
    case SType::INT64:   return resolve1<int64_t>(opcode);
    case SType::FLOAT32: return resolve1<float>(opcode);
    case SType::FLOAT64: return resolve1<double>(opcode);
    case SType::STR32:   return resolve_str<uint32_t>(opcode);
    case SType::STR64:   return resolve_str<uint64_t>(opcode);
    default: break;
  }
  return nullptr;
}


static Column* unaryop(Op opcode, Column* arg)
{
  if (opcode == Op::PLUS) return arg->shallowcopy();
  arg->materialize();

  SType arg_type = arg->stype();
  SType res_type = arg_type;
  if (opcode == Op::ISNA) {
    res_type = SType::BOOL;
  } else if (arg_type == SType::BOOL && opcode == Op::MINUS) {
    res_type = SType::INT8;
  } else if (opcode == Op::EXP || opcode == Op::LOGE ||
             opcode == Op::LOG10) {
    res_type = SType::FLOAT64;
  } else if (opcode == Op::LEN) {
    res_type = arg_type == SType::STR32? SType::INT32 : SType::INT64;
  }
  void* params[2];
  params[0] = arg;
  params[1] = Column::new_data_column(res_type, arg->nrows);

  mapperfn fn = resolve0(arg_type, opcode);
  if (!fn) {
    throw RuntimeError()
      << "Unable to apply unary op " << int(opcode) << " to column(stype="
      << arg_type << ")";
  }

  (*fn)(0, static_cast<int64_t>(arg->nrows), params);

  return static_cast<Column*>(params[1]);
}




//------------------------------------------------------------------------------
// expr_unaryop
//------------------------------------------------------------------------------

expr_unaryop::expr_unaryop(pexpr&& a, size_t op)
  : arg(std::move(a)), opcode(static_cast<Op>(op))
{
  xassert(op >= UNOP_FIRST && op <= UNOP_LAST);
}


bool expr_unaryop::is_negated_expr() const {
  return opcode == Op::MINUS;
}

pexpr expr_unaryop::get_negated_expr() {
  pexpr res;
  if (opcode == Op::MINUS) {
    res = std::move(arg);
  }
  return res;
}


SType expr_unaryop::resolve(const workframe& wf) {
  SType arg_stype = arg->resolve(wf);
  size_t op_id = id(opcode, arg_stype);
  if (unop_rules.count(op_id) == 0) {
    throw TypeError() << "Unary operator `"
        << unop_names[static_cast<size_t>(opcode)]
        << "` cannot be applied to a column with stype `" << arg_stype << "`";
  }
  return unop_rules.at(op_id);
}


GroupbyMode expr_unaryop::get_groupby_mode(const workframe& wf) const {
  return arg->get_groupby_mode(wf);
}


colptr expr_unaryop::evaluate_eager(workframe& wf) {
  auto arg_res = arg->evaluate_eager(wf);
  return colptr(unaryop(opcode, arg_res.get()));
}




//------------------------------------------------------------------------------
// one-time initialization
//------------------------------------------------------------------------------

static size_t id(Op opcode) {
  return static_cast<size_t>(opcode);
}
static size_t id(Op opcode, SType st1) {
  return (static_cast<size_t>(opcode) << 8) + static_cast<size_t>(st1);
}

void init_unops() {
  constexpr SType bool8 = SType::BOOL;
  constexpr SType int8  = SType::INT8;
  constexpr SType int16 = SType::INT16;
  constexpr SType int32 = SType::INT32;
  constexpr SType int64 = SType::INT64;
  constexpr SType flt32 = SType::FLOAT32;
  constexpr SType flt64 = SType::FLOAT64;
  constexpr SType str32 = SType::STR32;
  constexpr SType str64 = SType::STR64;

  using styvec = std::vector<SType>;
  styvec integer_stypes = {int8, int16, int32, int64};
  styvec numeric_stypes = {bool8, int8, int16, int32, int64, flt32, flt64};
  styvec string_types = {str32, str64};
  styvec all_stypes = {bool8, int8, int16, int32, int64,
                       flt32, flt64, str32, str64};

  for (SType st : all_stypes) {
    unop_rules[id(Op::ISNA, st)] = bool8;
  }
  for (SType st : integer_stypes) {
    unop_rules[id(Op::INVERT, st)] = st;
  }
  for (SType st : numeric_stypes) {
    unop_rules[id(Op::MINUS, st)] = st;
    unop_rules[id(Op::PLUS, st)] = st;
    unop_rules[id(Op::ABS, st)] = st;
    unop_rules[id(Op::EXP, st)] = flt64;
    unop_rules[id(Op::LOGE, st)] = flt64;
    unop_rules[id(Op::LOG10, st)] = flt64;
  }
  unop_rules[id(Op::MINUS, bool8)] = int8;
  unop_rules[id(Op::PLUS, bool8)] = int8;
  unop_rules[id(Op::ABS, bool8)] = int8;
  unop_rules[id(Op::INVERT, bool8)] = bool8;
  unop_rules[id(Op::LEN, str32)] = int32;
  unop_rules[id(Op::LEN, str64)] = int64;

  unop_names.resize(1 + id(Op::LEN));
  unop_names[id(Op::ISNA)]   = "isna";
  unop_names[id(Op::MINUS)]  = "-";
  unop_names[id(Op::PLUS)]   = "+";
  unop_names[id(Op::INVERT)] = "~";
  unop_names[id(Op::ABS)]    = "abs";
  unop_names[id(Op::EXP)]    = "exp";
  unop_names[id(Op::LOGE)]   = "log";
  unop_names[id(Op::LOG10)]  = "log10";
  unop_names[id(Op::LEN)]    = "len";
}




}}  // namespace dt::expr
