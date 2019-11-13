//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#ifndef dt_EXPR_EXPR_BINARYOP_h
#define dt_EXPR_EXPR_BINARYOP_h
#include <unordered_map>
#include "expr/expr.h"
#include "python/_all.h"
#include "python/args.h"
#include "utils/function.h"
namespace dt {
namespace expr {



class _binary_infos {
  public:
    using erased_func_t = void(*)();
    struct binfo {
      erased_func_t scalarfn;
      SType output_stype;
      SType lhs_cast_stype;
      SType rhs_cast_stype;
      size_t : 40;
    };

    _binary_infos();
    const binfo* get_info_n(Op opcode, SType stype1, SType stype2) const;
    const binfo* get_info_x(Op opcode, SType stype1, SType stype2) const;
    Op get_opcode_from_args(const py::PKArgs&) const;

  private:
    using binfo_index_t = size_t;
    using opinfo_index_t = size_t;
    std::unordered_map<binfo_index_t, binfo> infos;
    std::unordered_map<opinfo_index_t, std::string> names;
    Op current_opcode;  // used only when registering opcodes

    static constexpr opinfo_index_t id(Op) noexcept;
    static constexpr binfo_index_t id(Op, SType, SType) noexcept;

    void add_relop(Op op, const char* name);
};

extern _binary_infos binary_infos;


// Called once at module initialization
// TODO: remove
void init_binops();

Column binaryop(Op opcode, Column& lhs, Column& rhs);


}}
#endif
