//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include "utils/exceptions.h"



void py::Frame::integrity_check() {
  if (!dt) {
    throw AssertionError() << "py::Frame.dt is NULL";
  }

  dt->verify_integrity();

  if (stypes) {
    if (!py::robj(stypes).is_tuple()) {
      throw AssertionError() << "py::Frame.stypes is not a tuple";
    }
    auto stypes_tuple = py::robj(stypes).to_otuple();
    if (stypes_tuple.size() != dt->ncols) {
      throw AssertionError() << "len(.stypes) = " << stypes_tuple.size()
          << " is different from .ncols = " << dt->ncols;
    }
    for (size_t i = 0; i < dt->ncols; ++i) {
      SType col_stype = dt->columns[i]->stype();
      auto elem = stypes_tuple[i];
      auto eexp = info(col_stype).py_stype();
      if (elem != eexp) {
        throw AssertionError() << "Element " << i << " of .stypes is "
            << elem << ", but the column's stype is " << col_stype;
      }
    }
  }
  if (ltypes) {
    if (!py::robj(ltypes).is_tuple()) {
      throw AssertionError() << "py::Frame.ltypes is not a tuple";
    }
    auto ltypes_tuple = py::robj(ltypes).to_otuple();
    if (ltypes_tuple.size() != dt->ncols) {
      throw AssertionError() << "len(.ltypes) = " << ltypes_tuple.size()
          << " is different from .ncols = " << dt->ncols;
    }
    for (size_t i = 0; i < dt->ncols; ++i) {
      SType col_stype = dt->columns[i]->stype();
      auto elem = ltypes_tuple[i];
      auto eexp = info(col_stype).py_ltype();
      if (elem != eexp) {
        throw AssertionError() << "Element " << i << " of .ltypes is "
            << elem << ", but the column's ltype is " << col_stype;
      }
    }
  }
}
