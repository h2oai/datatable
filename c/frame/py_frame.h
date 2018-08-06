//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_FRAME_PYFRAME_h
#define dt_FRAME_PYFRAME_h
#include "python/ext_type.h"
#include "datatable.h"

namespace dt {


/**
 * [WIP]
 * This class is exposed to python via `datatable.lib.core.Frame`, but it is
 * not directly usable yet.
 */
class Frame : public PyObject {
  private:
    DataTable* dt;

  public:
    class Type : public py::ExtType<Frame> {
      public:
        static const char* classname();
        static const char* classdoc();
    };

    void m__dealloc__();
    void m__get_buffer__(Py_buffer* buf, int flags) const;
    void m__release_buffer__(Py_buffer* buf) const;
};



}  // namespace dt

#endif
