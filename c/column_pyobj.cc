//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include "column/sentinel_fw.h"
#include "python/obj.h"
#include "python/string.h"
#include "utils/assert.h"



PyObjectColumn::PyObjectColumn() : FwColumn<py::robj>() {
  _stype = SType::OBJ;
  mbuf.set_pyobjects(/*clear_data = */ true);
}

PyObjectColumn::PyObjectColumn(size_t nrows_) : FwColumn<py::robj>(nrows_) {
  _stype = SType::OBJ;
  mbuf.set_pyobjects(/*clear_data = */ true);
}

PyObjectColumn::PyObjectColumn(size_t nrows_, Buffer&& mb)
    : FwColumn<py::robj>(nrows_, std::move(mb))
{
  _stype = SType::OBJ;
  mbuf.set_pyobjects(/*clear_data = */ false);
}



bool PyObjectColumn::get_element(size_t i, py::robj* out) const {
  py::robj x = this->elements_r()[i];
  *out = x;
  return !x.is_none();
}



ColumnImpl* PyObjectColumn::materialize() {
  return this;
}
