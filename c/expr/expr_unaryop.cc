//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <cmath>
#include "column/func_unary.h"
#include "column/virtual.h"
#include "expr/expr_unaryop.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "datatablemodule.h"
#include "types.h"
namespace dt {
namespace expr {

// Singleton instance
unary_infos unary_library;



//------------------------------------------------------------------------------
// Virtual-column functions
//------------------------------------------------------------------------------


template <SType SI, SType SO, element_t<SO>(*FN)(element_t<SI>)>
Column vcol_factory(Column&& arg) {
  size_t nrows = arg.nrows();
  return Column(
      new FuncUnary1_ColumnImpl<element_t<SI>, element_t<SO>>(
              std::move(arg), FN, nrows, SO)
  );
}


template <SType SI, SType SO, bool(*FN)(read_t<SI>, bool, read_t<SO>*)>
Column vcol_factory2(Column&& arg) {
  size_t nrows = arg.nrows();
  return Column(
      new FuncUnary2_ColumnImpl<read_t<SI>, read_t<SO>>(
              std::move(arg), FN, nrows, SO)
  );
}


template <SType SI, bool(*FN)(element_t<SI>)>
Column vcol_factory_bool(Column&& arg) {
  using TI = element_t<SI>;
  size_t nrows = arg.nrows();
  return Column(
      new FuncUnary1_ColumnImpl<TI, int8_t>(
              std::move(arg), reinterpret_cast<int8_t(*)(TI)>(FN),
              nrows, SType::BOOL)
  );
}


static Column vcol_id(Column&& arg) {
  return std::move(arg);
}




//------------------------------------------------------------------------------
// Operator implementations
//------------------------------------------------------------------------------


[[ noreturn ]] inline static void op_notimpl() {
  throw NotImplError() << "This operation is not implemented yet";
}



/*
inline static bool op_str_len_ascii(CString str, bool isvalid, int64_t* out) {
  *out = str.size;
  return isvalid;
}
*/

static bool op_str_len_unicode(CString str, bool isvalid, int64_t* out) {
  if (isvalid) {
    int64_t len = 0;
    const uint8_t* ch = reinterpret_cast<const uint8_t*>(str.ch);
    const uint8_t* end = ch + str.size;
    while (ch < end) {
      uint8_t c = *ch;
      ch += (c < 0x80)? 1 :
            ((c & 0xE0) == 0xC0)? 2 :
            ((c & 0xF0) == 0xE0)? 3 :  4;
      len++;
    }
    *out = len;
  }
  return isvalid;
}




//------------------------------------------------------------------------------
// unary_pyfn
//------------------------------------------------------------------------------

static py::oobj make_pyexpr(Op opcode, py::oobj arg) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op),
                                        py::otuple(arg) });
}

static py::oobj make_pyexpr(Op opcode, py::otuple args, py::otuple params) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op), args, params });
}


// This helper function will apply `opcode` to the entire frame, and
// return the resulting frame (same shape as the original).
static py::oobj process_frame(Op opcode, py::robj arg) {
  xassert(arg.is_frame());
  auto frame = static_cast<py::Frame*>(arg.to_borrowed_ref());
  DataTable* dt = frame->get_datatable();

  py::olist columns(dt->ncols());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    py::oobj col_selector = make_pyexpr(Op::COL,
                                        py::otuple{py::oint(i)},
                                        py::otuple{py::oint(0)});
    columns.set(i, make_pyexpr(opcode, col_selector));
  }

  py::oobj res = frame->m__getitem__(py::otuple{ py::None(), columns });
  DataTable* res_dt = res.to_datatable();
  res_dt->copy_names_from(*dt);
  return res;
}


py::oobj unary_pyfn(const py::PKArgs& args) {
  using uinfo = unary_infos::uinfo;
  Op opcode = unary_library.get_opcode_from_args(args);

  py::robj x = args[0].to_robj();
  if (x.is_dtexpr()) {
    return make_pyexpr(opcode, x);
  }
  if (x.is_frame()) {
    return process_frame(opcode, x);
  }
  if (x.is_int()) {
    int64_t v = x.to_int64_strict();
    const uinfo* info = unary_library.get_infon(opcode, SType::INT64);
    if (info && info->output_stype == SType::INT64) {
      auto res = reinterpret_cast<int64_t(*)(int64_t)>(info->scalarfn)(v);
      return py::oint(res);
    }
    if (info && info->output_stype == SType::BOOL) {
      auto res = reinterpret_cast<bool(*)(int64_t)>(info->scalarfn)(v);
      return py::obool(res);
    }
    goto process_as_float;
  }
  if (x.is_float() || x.is_none()) {
    process_as_float:
    double v = x.to_double();
    const uinfo* info = unary_library.get_infon(opcode, SType::FLOAT64);
    if (info && info->output_stype == SType::FLOAT64) {
      auto res = reinterpret_cast<double(*)(double)>(info->scalarfn)(v);
      return py::ofloat(res);
    }
    if (info && info->output_stype == SType::BOOL) {
      auto res = reinterpret_cast<bool(*)(double)>(info->scalarfn)(v);
      return py::obool(res);
    }
    if (!x.is_none()) {
      throw TypeError() << "Function `" << args.get_short_name()
        << "` cannot be applied to a numeric argument";
    }
    // Fall-through if x is None
  }
  if (x.is_string() || x.is_none()) {

  }
  if (!x) {
    throw TypeError() << "Function `" << args.get_short_name() << "()` takes "
        "exactly one argument, 0 given";
  }
  throw TypeError() << "Function `" << args.get_short_name() << "()` cannot "
      "be applied to an argument of type " << x.typeobj();
}




//------------------------------------------------------------------------------
// Floating-point functions
//------------------------------------------------------------------------------

static py::PKArgs args_floor(
  1, 0, 0, false, false, {"x"}, "floor",
  "The largest integer value not greater than `x`.");

static py::PKArgs args_trunc(
  1, 0, 0, false, false, {"x"}, "trunc",
  "The nearest integer value not greater than `x` in absolute value.");

static py::PKArgs args_len(
  1, 0, 0, false, false, {"s"}, "len",
  "The length of the string `s`.");




//------------------------------------------------------------------------------
// unary_infos
//------------------------------------------------------------------------------
using uinfo = unary_infos::uinfo;

const uinfo* unary_infos::get_infon(Op op, SType input_stype) const {
  size_t entry_id = id(op, input_stype);
  return info.count(entry_id)? &info.at(entry_id) : nullptr;
}

const uinfo& unary_infos::get_infox(Op op, SType input_stype) const {
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

Op unary_infos::get_opcode_from_args(const py::PKArgs& args) const {
  return opcodes.at(&args);
}



constexpr size_t unary_infos::id(Op op) noexcept {
  return static_cast<size_t>(op) - UNOP_FIRST;
}

constexpr size_t unary_infos::id(Op op, SType stype) noexcept {
  return static_cast<size_t>(op) * DT_STYPES_COUNT +
         static_cast<size_t>(stype);
}

void unary_infos::add_op(Op opcode, const char* name, const py::PKArgs* args) {
  names[id(opcode)] = name;
  opcodes[args] = opcode;
}


template <Op OP, SType SI, SType SO, read_t<SO>(*FN)(read_t<SI>)>
void unary_infos::add() {
  constexpr size_t entry_id = id(OP, SI);
  xassert(info.count(entry_id) == 0);
  info[entry_id] = {
    /* scalarfn = */ reinterpret_cast<erased_func_t>(FN),
    /* vcolfn =   */ vcol_factory<SI, SO, FN>,
    /* outstype = */ SO,
    /* caststype= */ SType::VOID
  };
}


template <Op OP, SType SI, SType SO, bool(*FN)(read_t<SI>, bool, read_t<SO>*)>
void unary_infos::add() {
  constexpr size_t entry_id = id(OP, SI);
  xassert(info.count(entry_id) == 0);
  info[entry_id] = {
    /* scalarfn = */ op_notimpl,
    /* vcolfn =   */ vcol_factory2<SI, SO, FN>,
    /* outstype = */ SO,
    /* caststype= */ SType::VOID
  };
}


void unary_infos::add_copy(Op op, SType input_stype, SType output_stype) {
  size_t entry_id = id(op, input_stype);
  xassert(info.count(entry_id) == 0);
  SType cast_stype = (input_stype == output_stype)? SType::VOID : output_stype;
  info[entry_id] = {nullptr, vcol_id, output_stype, cast_stype};
}


template <float(*F32)(float), double(*F64)(double)>
inline void unary_infos::add_math(
    Op opcode, const char* name, const py::PKArgs& args)
{
  static SType integer_stypes[] = {SType::BOOL, SType::INT8, SType::INT16,
                                   SType::INT32, SType::INT64};
  auto s32 = reinterpret_cast<erased_func_t>(F32);
  auto s64 = reinterpret_cast<erased_func_t>(F64);
  auto v32 = vcol_factory<SType::FLOAT32, SType::FLOAT32, F32>;
  auto v64 = vcol_factory<SType::FLOAT64, SType::FLOAT64, F64>;
  names[id(opcode)] = name;
  opcodes[&args] = opcode;
  for (size_t i = 0; i < 5; ++i) {
    size_t entry_id = id(opcode, integer_stypes[i]);
    xassert(info.count(entry_id) == 0);
    info[entry_id] = {s64, v64, SType::FLOAT64, SType::FLOAT64};
  }
  size_t id_f32 = id(opcode, SType::FLOAT32);
  size_t id_f64 = id(opcode, SType::FLOAT64);
  xassert(info.count(id_f32) == 0 && info.count(id_f64) == 0);
  info[id_f32] = {s32, v32, SType::FLOAT32, SType::VOID};
  info[id_f64] = {s64, v64, SType::FLOAT64, SType::VOID};
}


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

  add_op(Op::FLOOR, "floor", &args_floor);
  add_copy(Op::FLOOR, bool8, flt64);
  add_copy(Op::FLOOR, int8,  flt64);
  add_copy(Op::FLOOR, int16, flt64);
  add_copy(Op::FLOOR, int32, flt64);
  add_copy(Op::FLOOR, int64, flt64);
  add<Op::FLOOR, flt32, flt32, std::floor>();
  add<Op::FLOOR, flt64, flt64, std::floor>();

  add_op(Op::TRUNC, "trunc", &args_trunc);
  add_copy(Op::TRUNC, bool8, flt64);
  add_copy(Op::TRUNC, int8,  flt64);
  add_copy(Op::TRUNC, int16, flt64);
  add_copy(Op::TRUNC, int32, flt64);
  add_copy(Op::TRUNC, int64, flt64);
  add<Op::TRUNC, flt32, flt32, std::trunc>();
  add<Op::TRUNC, flt64, flt64, std::trunc>();

  add_op(Op::LEN, "len", &args_len);
  add<Op::LEN, str32, int64, op_str_len_unicode>();
  add<Op::LEN, str64, int64, op_str_len_unicode>();
}

}}  // namespace dt::expr


void py::DatatableModule::init_unops() {
  using namespace dt::expr;
  ADD_FN(&unary_pyfn, args_floor);
  ADD_FN(&unary_pyfn, args_trunc);
}
