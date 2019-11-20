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




static py::PKArgs args_len(
  1, 0, 0, false, false, {"s"}, "len",
  "The length of the string `s`.");




//------------------------------------------------------------------------------
// unary_infos
//------------------------------------------------------------------------------
using uinfo = unary_infos::uinfo;

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


constexpr size_t unary_infos::id(Op op) noexcept {
  return static_cast<size_t>(op) - UNOP_FIRST;
}

constexpr size_t unary_infos::id(Op op, SType stype) noexcept {
  return static_cast<size_t>(op) * DT_STYPES_COUNT +
         static_cast<size_t>(stype);
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




unary_infos::unary_infos() {
  constexpr SType int64 = SType::INT64;
  constexpr SType str32 = SType::STR32;
  constexpr SType str64 = SType::STR64;

  add<Op::LEN, str32, int64, op_str_len_unicode>();
  add<Op::LEN, str64, int64, op_str_len_unicode>();
}

}}  // namespace dt::expr
