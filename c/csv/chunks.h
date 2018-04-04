//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_CSV_CHUNKS_h
#define dt_CSV_CHUNKS_h
#include <memory>           // std::unique_ptr
#include "utils/assert.h"


//------------------------------------------------------------------------------
// ChunkCoordinates struct
//------------------------------------------------------------------------------

/**
 * Helper struct containing the beginning / the end for a chunk.
 *
 * Additional flags `true_start` and `true_end` indicate whether the beginning /
 * end of the chunk are known with certainty or guessed.
 */
struct ChunkCoordinates {
  const char* start;
  const char* end;
  bool true_start;
  bool true_end;
  size_t : 48;

  ChunkCoordinates()
    : start(nullptr), end(nullptr), true_start(false), true_end(false) {}
  ChunkCoordinates(const char* s, const char* e)
    : start(s), end(e), true_start(false), true_end(false) {}
  ChunkCoordinates& operator=(const ChunkCoordinates& cc) {
    start = cc.start;
    end = cc.end;
    true_start = cc.true_start;
    true_end = cc.true_end;
    return *this;
  }

  operator bool() const { return end == nullptr; }
};



//------------------------------------------------------------------------------
// Base ChunkOrganizer
//------------------------------------------------------------------------------
class LocalParseContext;

/**
 * Helper class whose responsibility is to determine how the input should be
 * split into chunks, based on the size of the input, number of available
 * threads, and the estimated line length.
 *
 * If the input is sufficiently small, then instead of making the chunks too
 * small in size, this class will rather reduce the number of threads used.
 *
 * Call `get_nthreads()` to find the recommended number of threads. If OpenMP
 * allocates less threads than requested, then you can call `set_nthreads()` to
 * make this class re-evaluate the chunking strategy based on this information.
 */
class ChunkOrganizer {
  private:
    size_t chunkSize;
    size_t chunkCount;
    const char* inputStart;
    const char* inputEnd;
    const char* lastChunkEnd;
    double lineLength;
    int nThreads;
    int : 32;

  public:
    ChunkOrganizer(const char* start, const char* end, int nth, double len);
    ChunkOrganizer(const ChunkOrganizer&) = delete;
    ChunkOrganizer& operator=(const ChunkOrganizer&) = delete;
    virtual ~ChunkOrganizer() {}

    size_t get_nchunks() const;
    int get_nthreads() const;
    void set_nthreads(int n);

    /**
     * Determine coordinates (start and end) of the `i`-th chunk. The index `i`
     * must be in the range [0..chunkCount).
     *
     * The optional `LocalParseContext` instance may be needed for some
     * implementations of `ChunkOrganizer` in order to perform additional
     * parsing using a thread-local context.
     *
     * The method `compute_chunk_boundaries()` may be called in-parallel,
     * assuming that different invocation receive different `ctx` objects.
     */
    ChunkCoordinates compute_chunk_boundaries(
      size_t i, LocalParseContext* ctx = nullptr) const;

    /**
     * Ensure that the chunks were placed properly. This method must be called
     * from the #ordered section. It takes two arguments: the *actual*
     * coordinates of the chunk just read; and the coordinates that were
     * *expected*. If the chunk was chunk was ordered properly, than this
     * method returns true. Otherwise, it updates the expected coordinates
     * `xcc` and returns false. The caller is expected to re-parse the chunk
     * with the updated coords, and then call this method again.
     */
    bool is_ordered(const ChunkCoordinates& acc, ChunkCoordinates& xcc);

    void unorder_chunk(const ChunkCoordinates& cc);

    /**
     * Return the fraction of the input that was parsed, as a number between
     * 0 and 1.0.
     */
    double work_done_amount() const;


  protected:
    /**
     * This method can be overridden in derived classes in order to implement
     * more advanced chunk boundaries detection. This method will be called from
     * within `compute_chunk_boundaries()` only.
     * This method should modify `cc` by-reference; making sure not to alter
     * `start` / `end` if the flags `true_start` / `true_end` are set.
     */
    virtual void adjust_chunk_coordinates(
      ChunkCoordinates& cc, LocalParseContext* ctx) const;

  private:
    void determine_chunking_strategy();
};


typedef std::unique_ptr<ChunkOrganizer> ChunkOrganizerPtr;



//------------------------------------------------------------------------------
// Fread ChunkOrganizer
//------------------------------------------------------------------------------
class FreadReader;
struct FreadTokenizer;


class FreadChunkOrganizer : public ChunkOrganizer {
  private:
    const FreadReader& fr;

  public:
    FreadChunkOrganizer(const char* start, const char* end,
                        const FreadReader& f);

    bool next_good_line_start(
      const ChunkCoordinates& cc, FreadTokenizer& tokenizer) const;

  protected:
    void adjust_chunk_coordinates(
      ChunkCoordinates& cc, LocalParseContext* ctx) const override;
};



#endif
