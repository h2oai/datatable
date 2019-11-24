
//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include "python/xobject.h"

namespace py {

XTypeMaker::ConstructorTag XTypeMaker::constructor_tag;
XTypeMaker::DestructorTag XTypeMaker::destructor_tag;
XTypeMaker::GetSetTag XTypeMaker::getset_tag;
XTypeMaker::MethodTag XTypeMaker::method_tag;
XTypeMaker::Method0Tag XTypeMaker::method0_tag;
XTypeMaker::ReprTag XTypeMaker::repr_tag;
XTypeMaker::StrTag XTypeMaker::str_tag;
XTypeMaker::GetitemTag XTypeMaker::getitem_tag;
XTypeMaker::SetitemTag XTypeMaker::setitem_tag;
XTypeMaker::BuffersTag XTypeMaker::buffers_tag;
XTypeMaker::IterTag XTypeMaker::iter_tag;
XTypeMaker::NextTag XTypeMaker::next_tag;

} // namespace py