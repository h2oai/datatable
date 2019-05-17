//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <cmath>
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
// Mapper functions
//------------------------------------------------------------------------------

template<typename T, T(*OP)(T)>
void map1(size_t nrows, const T* inp, T* out) {
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = OP(inp[i]);
    });
}


template<typename IT, typename OT, OT(*OP)(IT)>
void map11(size_t nrows, const IT* inp, OT* out) {
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = OP(inp[i]);
    });
}


template<typename IT, typename OT>
void map_str_len(size_t nrows, const IT* inp, OT* out) {
  inp++;
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = ISNA<IT>(inp[i])
                  ? GETNA<OT>()
                  : static_cast<OT>((inp[i] - inp[i-1]) & ~GETNA<IT>());
    });
}


template<typename T>
void map_str_isna(size_t nrows, const T* inp, int8_t* out) {
  inp++;
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = ISNA<T>(inp[i]);
    });
}


template <int8_t VAL>
void set_const(size_t nrows, const void*, int8_t* out) {
  dt::parallel_for_static(nrows,
    [=](size_t i){
      out[i] = VAL;
    });
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
inline static bool op_isna(T x) { return ISNA<T>(x); }

template <typename T>
inline static bool op_notna(T x) { return !ISNA<T>(x); }

template <typename T>
inline static bool op_false(T) { return false; }


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
  return unary_library.get_infox(opcode, input_stype).output_stype;
}


GroupbyMode expr_unaryop::get_groupby_mode(const workframe& wf) const {
  return arg->get_groupby_mode(wf);
}


// In this method we use the following shortcut:
// if the `input_column` evaluated from `arg` is "writable" (meaning that
// its reference-counter is 1, it is allocated in RAM and not marker reaonly),
// then this column's memory buffer can be reused to create the output column.
// Indeed, if nobody else holds a reference to the same memory range, then it
// will be deallocated at the end of this function anyways, together with the
// `input_column`.
// This works only if each element in the output array corresponds to an
// element in the input at exactly the same memory location. Thus, this
// optimization is only applicable when the both columns are "fixed-width" and
// element sizes in the input and the output columns are the same. Notably,
// if the input is a string column, then it cannot be safely repurposed into
// an integer output column because each "element" of the input actually
// consists of 2 entries: the offsets of the start and the end of a string.
//
colptr expr_unaryop::evaluate_eager(workframe& wf) {
  auto input_column = arg->evaluate_eager(wf);

  auto input_stype = input_column->stype();
  const auto& ui = unary_library.get_infox(opcode, input_stype);
  if (ui.fn == nullptr) {
    if (input_stype == ui.output_stype) {
      return colptr(input_column->shallowcopy());
    } else {
      return colptr(input_column->cast(ui.output_stype));
    }
  }
  if (ui.cast_stype != SType::VOID && input_stype != ui.cast_stype) {
    input_column = colptr(input_column->cast(ui.cast_stype));
  }
  input_column->materialize();

  size_t nrows = input_column->nrows;
  const MemoryRange& input_mbuf = input_column->data_buf();
  const void* inp = input_mbuf.rptr();

  MemoryRange output_mbuf;
  void* out = nullptr;
  size_t out_elemsize = info(ui.output_stype).elemsize();
  if (input_mbuf.is_writable() &&
      input_column->is_fixedwidth() &&
      input_column->elemsize() == out_elemsize) {
    // The address `out` must be taken *before* `output_mbuf` is initialized,
    // since the `.xptr()` method checks that the reference counter is 1.
    out = input_mbuf.xptr();
    output_mbuf = MemoryRange(input_mbuf);
  }
  else {
    output_mbuf = MemoryRange::mem(out_elemsize * nrows);
    out = output_mbuf.xptr();
  }
  auto output_column = colptr(
      Column::new_mbuf_column(ui.output_stype, std::move(output_mbuf)));

  ui.fn(nrows, inp, out);

  return output_column;
}



//------------------------------------------------------------------------------
// unary_pyfn
//------------------------------------------------------------------------------

// This helper function will apply `opcode` to the entire frame, and
// return the resulting frame (same shape as the original).
static py::oobj process_frame(Op opcode, py::robj arg) {
  xassert(arg.is_frame());
  auto frame = static_cast<py::Frame*>(arg.to_borrowed_ref());
  DataTable* dt = frame->get_datatable();

  py::olist columns(dt->ncols);
  for (size_t i = 0; i < dt->ncols; ++i) {
    py::oobj col_selector = make_pyexpr(Op::COL, py::oint(0), py::oint(i));
    columns.set(i, make_pyexpr(opcode, col_selector));
  }

  py::oobj res = frame->m__getitem__(py::otuple{ py::None(), columns });
  DataTable* res_dt = res.to_datatable();
  res_dt->copy_names_from(dt);
  return res;
}


py::oobj unary_pyfn(const py::PKArgs& args) {
  using uinfo = unary_infos::uinfo;
  Op opcode = unary_library.resolve_opcode(args);

  py::robj arg = args[0].to_robj();
  if (arg.is_dtexpr()) {
    return make_pyexpr(opcode, arg);
  }
  if (arg.is_frame()) {
    return process_frame(opcode, arg);
  }
  if (arg.is_float() || arg.is_none()) {
    double v = arg.to_double();
    const uinfo* info = unary_library.get_infon(opcode, SType::FLOAT64);
    if (info && info->output_stype == SType::FLOAT64) {
      auto res = info->scalarfn.d_d(v);
      return py::ofloat(res);
    }
    if (info && info->output_stype == SType::BOOL) {
      auto res = info->scalarfn.d_b(v);
      return py::obool(res);
    }
    if (!arg.is_none()) {
      throw TypeError() << "Function `" << args.get_short_name()
        << "` cannot be applied to a float argument";
    }
    // Fall-through if arg is None
  }
  if (arg.is_int() || arg.is_none()) {
    int64_t v = arg.to_int64_strict();
    const uinfo* info = unary_library.get_infon(opcode, SType::INT64);
    if (info && info->output_stype == SType::INT64) {
      auto res = info->scalarfn.l_l(v);
      return py::oint(res);
    }
    if (info && info->output_stype == SType::BOOL) {
      auto res = info->scalarfn.l_b(v);
      return py::obool(res);
    }
    if (!arg.is_none()) {
      throw TypeError() << "Function `" << args.get_short_name()
        << "` cannot be applied to an integer argument";
    }
  }
  if (arg.is_string() || arg.is_none()) {

  }
  if (!arg) {
    throw TypeError() << "Function `" << args.get_short_name() << "()` takes "
        "exactly one argument, 0 given";
  }
  throw TypeError() << "Function `" << args.get_short_name() << "()` cannot "
      "be applied to an argument of type " << arg.typeobj();
}



//------------------------------------------------------------------------------
// Args for python functions
//------------------------------------------------------------------------------

static py::PKArgs args_abs(
  1, 0, 0, false, false, {"x"}, "abs",
  "Absolute value of a number.");

static py::PKArgs args_isna(
  1, 0, 0, false, false, {"x"}, "isna",
  "Returns True if the argument is NA, and False otherwise.");

static py::PKArgs args_isfinite(
  1, 0, 0, false, false, {"x"}, "isfinite",
R"(Returns True if the argument has a finite value, and
False if the argument is +/- infinity or NaN.)");

static py::PKArgs args_isinf(
  1, 0, 0, false, false, {"x"}, "isinf",
  "Returns True if the argument is +/- infinity, and False otherwise.");



//------------------------------------------------------------------------------
// unary_infos
//------------------------------------------------------------------------------

#define U reinterpret_cast<unary_func_t>

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

  set_name(Op::UPLUS, "+");
  add(Op::UPLUS, bool8, int8,  nullptr);
  add(Op::UPLUS, int8,  int8,  nullptr);
  add(Op::UPLUS, int16, int16, nullptr);
  add(Op::UPLUS, int32, int32, nullptr);
  add(Op::UPLUS, int64, int64, nullptr);
  add(Op::UPLUS, flt32, flt32, nullptr);
  add(Op::UPLUS, flt64, flt64, nullptr);

  add(Op::UMINUS, bool8, int8,  U(map11<int8_t,  int8_t,  op_minus<int8_t>>));
  add(Op::UMINUS, int8,  int8,  U(map11<int8_t,  int8_t,  op_minus<int8_t>>));
  add(Op::UMINUS, int16, int16, U(map11<int16_t, int16_t, op_minus<int16_t>>));
  add(Op::UMINUS, int32, int32, U(map11<int32_t, int32_t, op_minus<int32_t>>));
  add(Op::UMINUS, int64, int64, U(map11<int64_t, int64_t, op_minus<int64_t>>));
  add(Op::UMINUS, flt32, flt32, U(map11<float,   float,   op_minus<float>>));
  add(Op::UMINUS, flt64, flt64, U(map11<double,  double,  op_minus<double>>));

  add(Op::UINVERT, bool8, bool8, U(map11<int8_t,  int8_t,  op_invert_bool>));
  add(Op::UINVERT, int8,  int8,  U(map11<int8_t,  int8_t,  op_inverse<int8_t>>));
  add(Op::UINVERT, int16, int16, U(map11<int16_t, int16_t, op_inverse<int16_t>>));
  add(Op::UINVERT, int32, int32, U(map11<int32_t, int32_t, op_inverse<int32_t>>));
  add(Op::UINVERT, int64, int64, U(map11<int64_t, int64_t, op_inverse<int64_t>>));

  register_op(Op::ABS, "abs", args_abs,
    [=]{
      add_copy_converter(bool8, int8);
      add_converter(map1<int8_t,  op_abs<int8_t>>);
      add_converter(map1<int16_t, op_abs<int16_t>>);
      add_converter(map1<int32_t, op_abs<int32_t>>);
      add_converter(map1<int64_t, op_abs<int64_t>>);
      add_converter(map1<float,   std::abs>);
      add_converter(map1<double,  std::abs>);
      add_scalarfn_l(op_abs<int64_t>);
      add_scalarfn_d(std::abs);
    });

  register_op(Op::ISNA, "isna", args_isna,
    [=]{
      add_converter(bool8, bool8, U(map11<int8_t,  bool, op_isna<int8_t>>));
      add_converter(int8,  bool8, U(map11<int8_t,  bool, op_isna<int8_t>>));
      add_converter(int16, bool8, U(map11<int16_t, bool, op_isna<int16_t>>));
      add_converter(int32, bool8, U(map11<int32_t, bool, op_isna<int32_t>>));
      add_converter(int64, bool8, U(map11<int64_t, bool, op_isna<int64_t>>));
      add_converter(flt32, bool8, U(map11<float,   bool, std::isnan>));
      add_converter(flt64, bool8, U(map11<double,  bool, std::isnan>));
      add_converter(str32, bool8, U(map_str_isna<uint32_t>));
      add_converter(str64, bool8, U(map_str_isna<uint64_t>));
      add_scalarfn_l(op_isna<int64_t>);
      add_scalarfn_d(std::isnan);
    });

  register_op(Op::ISFINITE, "isfinite", args_isfinite,
    [&]{
      add_converter(bool8, bool8, U(map11<int8_t,  bool, op_notna<int8_t>>));
      add_converter(int8,  bool8, U(map11<int8_t,  bool, op_notna<int8_t>>));
      add_converter(int16, bool8, U(map11<int16_t, bool, op_notna<int16_t>>));
      add_converter(int32, bool8, U(map11<int32_t, bool, op_notna<int32_t>>));
      add_converter(int64, bool8, U(map11<int64_t, bool, op_notna<int64_t>>));
      add_converter(flt32, bool8, U(map11<float,   bool, std::isfinite>));
      add_converter(flt64, bool8, U(map11<double,  bool, std::isfinite>));
      add_scalarfn_l(op_notna<int64_t>);
      add_scalarfn_d(std::isfinite);
    });

  register_op(Op::ISINF, "isinf", args_isinf,
    [&]{
      add_converter(bool8, bool8, U(map11<int8_t,  bool, op_false<int8_t>>));
      add_converter(int8,  bool8, U(map11<int8_t,  bool, op_false<int8_t>>));
      add_converter(int16, bool8, U(map11<int16_t, bool, op_false<int16_t>>));
      add_converter(int32, bool8, U(map11<int32_t, bool, op_false<int32_t>>));
      add_converter(int64, bool8, U(map11<int64_t, bool, op_false<int64_t>>));
      add_converter(flt32, bool8, U(map11<float,   bool, std::isinf>));
      add_converter(flt64, bool8, U(map11<double,  bool, std::isinf>));
      add_scalarfn_l(op_false<int64_t>);
      add_scalarfn_d(std::isinf);
    });

  add(Op::CEIL, bool8, int8,  nullptr);
  add(Op::CEIL, int8,  int8,  nullptr);
  add(Op::CEIL, int16, int16, nullptr);
  add(Op::CEIL, int32, int32, nullptr);
  add(Op::CEIL, int64, int64, nullptr);
  add(Op::CEIL, flt32, flt32, U(map11<float,  float,  std::ceil>));
  add(Op::CEIL, flt64, flt64, U(map11<double, double, std::ceil>));

  add(Op::FLOOR, bool8, int8,  nullptr);
  add(Op::FLOOR, int8,  int8,  nullptr);
  add(Op::FLOOR, int16, int16, nullptr);
  add(Op::FLOOR, int32, int32, nullptr);
  add(Op::FLOOR, int64, int64, nullptr);
  add(Op::FLOOR, flt32, flt32, U(map11<float,  float,  std::floor>));
  add(Op::FLOOR, flt64, flt64, U(map11<double, double, std::floor>));

  add(Op::TRUNC, bool8, int8,  nullptr);
  add(Op::TRUNC, int8,  int8,  nullptr);
  add(Op::TRUNC, int16, int16, nullptr);
  add(Op::TRUNC, int32, int32, nullptr);
  add(Op::TRUNC, int64, int64, nullptr);
  add(Op::TRUNC, flt32, flt32, U(map11<float,  float,  std::trunc>));
  add(Op::TRUNC, flt64, flt64, U(map11<double, double, std::trunc>));

  add(Op::LEN, str32, int32, U(map_str_len<uint32_t, int32_t>));
  add(Op::LEN, str64, int64, U(map_str_len<uint64_t, int64_t>));

  set_name(Op::UMINUS, "-");
  set_name(Op::UINVERT, "~");
  set_name(Op::ISNA, "isna");
  set_name(Op::ISFINITE, "isfinite");
  set_name(Op::ISINF, "isinf");
  set_name(Op::CEIL, "ceil");
  set_name(Op::FLOOR, "floor");
  set_name(Op::TRUNC, "trunc");
  set_name(Op::LEN, "len");
}

#undef U


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
                      unary_func_t fn, SType cast)
{
  size_t entry_id = id(op, input_stype);
  xassert(info.count(entry_id) == 0);
  info[entry_id] = {fn, {nullptr}, output_stype, cast};
}


const unary_infos::uinfo* unary_infos::get_infon(Op op, SType input_stype) const
{
  size_t entry_id = id(op, input_stype);
  return info.count(entry_id)? &info.at(entry_id) : nullptr;
}


const unary_infos::uinfo& unary_infos::get_infox(Op op, SType input_stype) const
{
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


Op unary_infos::resolve_opcode(const py::PKArgs& args) const {
  return opcodes.at(&args);
}


void unary_infos::register_op(
  Op opcode, const std::string& name, const py::PKArgs& args,
  dt::function<void()> more)
{
  names[id(opcode)] = name;
  opcodes[&args] = opcode;
  current_opcode = opcode;
  more();
}

void unary_infos::add_converter(SType in, SType out, unary_func_t fn) {
  add(current_opcode, in, out, fn);
}

void unary_infos::add_copy_converter(SType in, SType out) {
  if (out == SType::VOID) out = in;
  add(current_opcode, in, out, nullptr);
}

void unary_infos::add_converter(void(*f)(size_t, const int8_t*, int8_t*)) {
  add(current_opcode, SType::INT8, SType::INT8, reinterpret_cast<unary_func_t>(f));
}

void unary_infos::add_converter(void(*f)(size_t, const int16_t*, int16_t*)) {
  add(current_opcode, SType::INT16, SType::INT16, reinterpret_cast<unary_func_t>(f));
}

void unary_infos::add_converter(void(*f)(size_t, const int32_t*, int32_t*)) {
  add(current_opcode, SType::INT32, SType::INT32, reinterpret_cast<unary_func_t>(f));
}

void unary_infos::add_converter(void(*f)(size_t, const int64_t*, int64_t*)) {
  add(current_opcode, SType::INT64, SType::INT64, reinterpret_cast<unary_func_t>(f));
}

void unary_infos::add_converter(void(*f)(size_t, const float*, float*)) {
  add(current_opcode, SType::FLOAT32, SType::FLOAT32, reinterpret_cast<unary_func_t>(f));
}

void unary_infos::add_converter(void(*f)(size_t, const double*, double*)) {
  add(current_opcode, SType::FLOAT64, SType::FLOAT64, reinterpret_cast<unary_func_t>(f));
}


void unary_infos::add_scalarfn_l(int64_t(*f)(int64_t)) {
  size_t entry_id = id(current_opcode, SType::INT64);
  xassert(info.count(entry_id) && !info[entry_id].scalarfn.l_l);
  info[entry_id].scalarfn.l_l = f;
}

void unary_infos::add_scalarfn_l(bool(*f)(int64_t)) {
  size_t entry_id = id(current_opcode, SType::INT64);
  xassert(info.count(entry_id) && !info[entry_id].scalarfn.l_b);
  info[entry_id].scalarfn.l_b = f;
}

void unary_infos::add_scalarfn_d(double(*f)(double)) {
  size_t entry_id = id(current_opcode, SType::FLOAT64);
  xassert(info.count(entry_id) && !info[entry_id].scalarfn.d_d);
  info[entry_id].scalarfn.d_d = f;
}

void unary_infos::add_scalarfn_d(bool(*f)(double)) {
  size_t entry_id = id(current_opcode, SType::FLOAT64);
  xassert(info.count(entry_id) && !info[entry_id].scalarfn.d_b);
  info[entry_id].scalarfn.d_b = f;
}


}}  // namespace dt::expr


void py::DatatableModule::init_unops() {
  using namespace dt::expr;
  ADD_FN(&unary_pyfn, args_abs);
  ADD_FN(&unary_pyfn, args_isna);
  ADD_FN(&unary_pyfn, args_isfinite);
  ADD_FN(&unary_pyfn, args_isinf);
}
