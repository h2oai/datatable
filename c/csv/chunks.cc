//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/chunks.h"
#include <algorithm>           // std::max
#include "csv/reader_fread.h"  // FreadReader, FreadLocalParseContext
#include "utils/assert.h"



//------------------------------------------------------------------------------
// Base ChunkOrganizer
//------------------------------------------------------------------------------

ChunkOrganizer::ChunkOrganizer(
    const char* start, const char* end, int nthreads, double meanLineLength
) : chunkSize(0),
    chunkCount(0),
    inputStart(start),
    inputEnd(end),
    lastChunkEnd(start),
    lineLength(meanLineLength),
    nThreads(nthreads)
{
  determine_chunking_strategy();
}


void ChunkOrganizer::determine_chunking_strategy() {
  size_t inputSize = static_cast<size_t>(inputEnd - inputStart);
  size_t size1000 = static_cast<size_t>(1000 * lineLength);
  size_t zThreads = static_cast<size_t>(nThreads);
  chunkSize = std::max<size_t>(size1000, 65536);
  chunkCount = std::max<size_t>(inputSize / chunkSize, 1);
  if (chunkCount > zThreads) {
    chunkCount = zThreads * (1 + (chunkCount - 1)/zThreads);
  } else {
    nThreads = static_cast<int>(chunkCount);
  }
  chunkSize = inputSize / chunkCount;
}



size_t ChunkOrganizer::get_nchunks() const {
  return chunkCount;
}

int ChunkOrganizer::get_nthreads() const {
  return nThreads;
}

void ChunkOrganizer::set_nthreads(int nth) {
  assert(nth > 0);
  nThreads = nth;
  determine_chunking_strategy();
}


ChunkCoordinates ChunkOrganizer::compute_chunk_boundaries(
  size_t i, LocalParseContext* ctx) const
{
  assert(i < chunkCount);
  ChunkCoordinates c;

  bool isFirstChunk = (i == 0);
  bool isLastChunk = (i == chunkCount - 1);

  c.start = nThreads == 1? lastChunkEnd : inputStart + i * chunkSize;
  c.end = isLastChunk? inputEnd : c.start + chunkSize;

  if (nThreads > 1) {
    if (!isFirstChunk) c.start = adjusted_chunk_start(c, ctx);
    if (!isLastChunk) c.end = adjusted_chunk_end(c, ctx);
  }

  // assert(inputStart <= c.start && c.start <= c.end && c.end <= inputEnd);
  return c;
}


bool ChunkOrganizer::is_ordered(
  const ChunkCoordinates& acc, ChunkCoordinates& xcc)
{
  if (acc.start == lastChunkEnd) {
    lastChunkEnd = acc.end;
    return true;
  } else {
    xcc.start = lastChunkEnd;
    return false;
  }
}


double ChunkOrganizer::work_done_amount() const {
  double done = static_cast<double>(lastChunkEnd - inputStart);
  double total = static_cast<double>(inputEnd - inputStart);
  return done / total;
}


const char* ChunkOrganizer::adjusted_chunk_start(
  const ChunkCoordinates& cc, LocalParseContext*) const
{
  return cc.start;
}

const char* ChunkOrganizer::adjusted_chunk_end(
  const ChunkCoordinates& cc, LocalParseContext*) const
{
  return cc.end;
}



//------------------------------------------------------------------------------
// Fread ChunkOrganizer
//------------------------------------------------------------------------------

FreadChunkOrganizer::FreadChunkOrganizer(
  const char* start, const char* end, const FreadReader& f
) : ChunkOrganizer(start, end,
                   f.get_nthreads(),
                   f.get_mean_line_len()), fr(f) {}



// Find the next "good line", in the sense that we find at least 5 lines
// with `ncols` fields from that point on.
bool FreadChunkOrganizer::next_good_line_start(
  const ChunkCoordinates& cc, FreadTokenizer& tokenizer) const
{
  int ncols = static_cast<int>(fr.get_ncols());
  bool fill = fr.get_fill();
  bool skipEmptyLines = fr.get_skip_empty_lines();
  const char*& ch = tokenizer.ch;
  ch = cc.start;
  const char* eof = cc.end;
  int attempts = 0;
  while (ch < eof && attempts++ < 10) {
    while (ch < eof && *ch != '\n' && *ch != '\r') ch++;
    if (ch == eof) break;
    tokenizer.skip_eol();  // updates `ch`
    // countfields() below moves the parse location, so store it in `ch1` in
    // order to revert to the current parsing location later.
    const char* ch1 = ch;
    int i = 0;
    for (; i < 5; ++i) {
      // `countfields()` advances `ch` to the beginning of the next line
      int n = tokenizer.countfields();
      if (n != ncols &&
          !(ncols == 1 && n == 0) &&
          !(skipEmptyLines && n == 0) &&
          !(fill && n < ncols)) break;
    }
    ch = ch1;
    // `i` is the count of consecutive consistent rows
    if (i == 5) return true;
  }
  return false;
}



// Adjust the beginning of the chunk so that it is guaranteed not to be
// on a newline.
const char* FreadChunkOrganizer::adjusted_chunk_start(
  const ChunkCoordinates& cc, LocalParseContext* ctx) const
{
  auto fctx = static_cast<FreadLocalParseContext*>(ctx);
  const char* start = cc.start;
  while (*start=='\n' || *start=='\r') start++;

  ChunkCoordinates ccc(start, cc.end);
  if (next_good_line_start(ccc, fctx->tokenizer)) {
    return fctx->tokenizer.ch;
  } else {
    return start;
  }
}



// Move the end of the chunk, similarly skipping all newline characters;
// plus 1 more character, thus guaranteeing that the entire next line will
// also "belong" to the current chunk (this because chunk reader stops at
// the first end of the line after `end`).
const char* FreadChunkOrganizer::adjusted_chunk_end(
  const ChunkCoordinates& cc, LocalParseContext*) const
{
  const char* end = cc.end;
  while (*end=='\n' || *end=='\r') end++;
  end++;
  return end;
}
