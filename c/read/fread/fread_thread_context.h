//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_READ_FREAD_THREAD_CONTEXT_h
#define dt_READ_FREAD_THREAD_CONTEXT_h
#include "parallel/shared_mutex.h"      // dt::shared_mutex
#include "read/columns.h"               // Columns
#include "read/fread/fread_tokenizer.h" // FreadTokenizer
#include "read/thread_context.h"        // ThreadContext

class FreadReader;
enum PT : uint8_t;
using ParserFnPtr = void (*)(dt::read::FreadTokenizer& ctx);

namespace dt {
namespace read {


/**
 * anchor
 *   Pointer that serves as a starting point for all offsets in "RelStr" fields.
 *
 */
class FreadThreadContext : public ThreadContext
{
  public:
    const char* anchor;
    int quoteRule;
    char quote;
    char sep;
    bool verbose;
    bool fill;
    bool skipEmptyLines;
    bool numbersMayBeNAs;
    int64_t : 48;
    double ttime_push;
    double ttime_read;
    PT* types;

    FreadReader& freader;
    Columns& columns;
    dt::shared_mutex& shmutex;
    FreadTokenizer tokenizer;
    const ParserFnPtr* parsers;

  public:
    FreadThreadContext(size_t bcols, size_t brows, FreadReader&, PT* types,
                       dt::shared_mutex&);
    FreadThreadContext(const FreadThreadContext&) = delete;
    FreadThreadContext& operator=(const FreadThreadContext&) = delete;
    virtual ~FreadThreadContext() override;

    virtual void push_buffers() override;
    void read_chunk(const ChunkCoordinates&, ChunkCoordinates&) override;
    void postprocess();
    void orderBuffer() override;
};


}  // namespace read
}  // namespace dt

#endif
