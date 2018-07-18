//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "read/fread/fread_parallel_reader.h"
#include "csv/reader_fread.h"    // FreadReader

namespace dt {
namespace read {


FreadParallelReader::FreadParallelReader(FreadReader& reader, PT* types_)
    : ParallelReader(reader, reader.get_mean_line_len()),
      f(reader),
      types(types_) {}


void FreadParallelReader::read_all() {
  ParallelReader::read_all();
  f.fo.read_data_nthreads = static_cast<size_t>(nthreads);
}


std::unique_ptr<ThreadContext> FreadParallelReader::init_thread_context() {
  size_t trows = std::max<size_t>(nrows_allocated / chunkCount, 4);
  size_t tcols = f.columns.nColumnsInBuffer();
  return std::unique_ptr<ThreadContext>(
            new FreadLocalParseContext(tcols, trows, f, types, shmutex));
}


// Find the next "good line", in the sense that we find at least 5 lines
// with `ncols` fields from that point on.
bool FreadParallelReader::next_good_line_start(
  const ChunkCoordinates& cc, FreadTokenizer& tokenizer) const
{
  int ncols = static_cast<int>(f.get_ncols());
  bool fill = f.fill;
  bool skipEmptyLines = f.skip_blank_lines;
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



void FreadParallelReader::adjust_chunk_coordinates(
  ChunkCoordinates& cc, ThreadContext* ctx) const
{
  // Adjust the beginning of the chunk so that it is guaranteed not to be
  // on a newline.
  if (!cc.start_exact) {
    auto fctx = static_cast<FreadLocalParseContext*>(ctx);
    const char* start = cc.start;
    while (*start=='\n' || *start=='\r') start++;
    cc.start = start;
    if (next_good_line_start(cc, fctx->tokenizer)) {
      cc.start = fctx->tokenizer.ch;
    }
  }
  // Move the end of the chunk, similarly skipping all newline characters;
  // plus 1 more character, thus guaranteeing that the entire next line will
  // also "belong" to the current chunk (this because chunk reader stops at
  // the first end of the line after `end`).
  if (!cc.end_exact) {
    const char* end = cc.end;
    while (*end=='\n' || *end=='\r') end++;
    cc.end = end + 1;
  }
}


}  // namespace read
}  // namespace dt
