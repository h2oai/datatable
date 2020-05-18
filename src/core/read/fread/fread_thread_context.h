//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#ifndef dt_READ_FREAD_THREAD_CONTEXT_h
#define dt_READ_FREAD_THREAD_CONTEXT_h
#include "read/fread/fread_tokenizer.h" // FreadTokenizer
#include "read/thread_context.h"        // ThreadContext
#include "_dt.h"
namespace dt {
namespace read {


/**
 * anchor
 *   Pointer that serves as a starting point for all offsets in "RelStr" fields.
 *
 */
class FreadThreadContext : public ThreadContext
{
  private:
    using ParserFnPtr = void(*)(FreadTokenizer& ctx);

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
    // PreFrame& preframe;
    FreadTokenizer tokenizer;
    const ParserFnPtr* parsers;

  public:
    FreadThreadContext(size_t bcols, size_t brows, FreadReader&, PT* types);
    FreadThreadContext(const FreadThreadContext&) = delete;
    FreadThreadContext& operator=(const FreadThreadContext&) = delete;
    ~FreadThreadContext() override;

    void read_chunk(const ChunkCoordinates&, ChunkCoordinates&) override;
    void postprocess();
    void order_buffer() override;
    void push_buffers() override;

    FreadTokenizer& get_tokenizer() { return tokenizer; }
};




}}  // namespace dt::read
#endif
