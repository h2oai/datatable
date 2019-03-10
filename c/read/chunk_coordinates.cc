//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "read/chunk_coordinates.h"
namespace dt {
namespace read {


ChunkCoordinates::ChunkCoordinates()
  : start(nullptr),
    end(nullptr),
    start_exact(false),
    end_exact(false) {}


ChunkCoordinates::ChunkCoordinates(const char* s, const char* e)
  : start(s),
    end(e),
    start_exact(false),
    end_exact(false) {}


ChunkCoordinates::operator bool() const {
  return end == nullptr;
}



}}  // namespace dt::read
