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



const char* ChunkCoordinates::get_start() const noexcept {
  return start;
}

const char* ChunkCoordinates::get_end() const noexcept {
  return end;
}


bool ChunkCoordinates::is_start_exact() const noexcept {
  return start_exact;
}

bool ChunkCoordinates::is_start_approximate() const noexcept {
  return !start_exact;
}

bool ChunkCoordinates::is_end_exact() const noexcept {
  return end_exact;
}

bool ChunkCoordinates::is_end_approximate() const noexcept {
  return !end_exact;
}



void ChunkCoordinates::set_start_exact(const char* ch) noexcept {
  start = ch;
  start_exact = true;
}

void ChunkCoordinates::set_end_exact(const char* ch) noexcept {
  end = ch;
  end_exact = true;
}

void ChunkCoordinates::set_start_approximate(const char* ch) noexcept {
  start = ch;
  start_exact = false;
}

void ChunkCoordinates::set_end_approximate(const char* ch) noexcept {
  end = ch;
  end_exact = false;
}



}}  // namespace dt::read
