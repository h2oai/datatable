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
  private:
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

    const char* get_start() const noexcept;
    const char* get_end() const noexcept;

    bool is_start_exact() const noexcept;
    bool is_end_exact() const noexcept;
    bool is_start_approximate() const noexcept;
    bool is_end_approximate() const noexcept;

    void set_start_exact(const char* ch) noexcept;
    void set_end_exact(const char* ch) noexcept;
    void set_start_approximate(const char* ch) noexcept;
    void set_end_approximate(const char* ch) noexcept;
};



}}  // namespace dt::read
#endif
