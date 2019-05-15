//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_OPTIONS_h
#define dt_OPTIONS_h
#include <functional>         // std::function
#include "python/arg.h"

namespace py { class oobj; }  // forward-declare py::oobj

namespace dt {


void register_option(const char* name,
                     std::function<py::oobj()> getter,
                     std::function<void(const py::Arg&)> setter,
                     const char* docstring);

void use_options_store(py::oobj options);

py::oobj get_option(const char* name);

void init_config_option(void* module);


}  // namespace dt
#endif
