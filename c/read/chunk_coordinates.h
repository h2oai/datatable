//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_READ_CHUNK_COORDINATES_h
#define dt_READ_CHUNK_COORDINATES_h
namespace dt {
namespace read {


/**
 * Helper struct containing the beginning / end for a chunk.
 *
 * Additional flags `start_exact` and `end_exact` indicate whether the beginning
 * and end of the chunk are known with certainty or were guessed.
 */
class ChunkCoordinates {
  public:
    const char* start;
    const char* end;
    bool start_exact;
    bool end_exact;
    long long : 48;

  public:
    ChunkCoordinates();
    ChunkCoordinates(const char* start_, const char* end_);
    ChunkCoordinates(const ChunkCoordinates&) = default;
    ChunkCoordinates& operator=(const ChunkCoordinates&) = default;

    operator bool() const;
};



}}  // namespace dt::read
#endif
