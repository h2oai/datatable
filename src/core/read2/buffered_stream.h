//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#ifndef dt_READ2_BUFFERED_STREAM_h
#define dt_READ2_BUFFERED_STREAM_h
#include <deque>
#include "buffer.h"
#include "read2/_declarations.h"
#include "read2/stream.h"
namespace dt {
namespace read2 {


// TODO:
//   1. Figure out how to run this in multi-threaded, parallel to the
//      standard ordered loop.
//   2. When there is no parallel loop running, this should still be
//      able to read and return data.
//



class BufferedStream : public Stream {
  using BufferedStreamPtr = std::unique_ptr<BufferedStream>;
  public:
    virtual ~BufferedStream();
    static BufferedStreamPtr fromBuffer(Buffer);
    static BufferedStreamPtr fromStream(std::unique_ptr<Stream>&& stream,
                                        size_t memoryLimit = (1<<30));

    // Request data chunk `[start; start+size)`. This function is
    // blocking: if the data is not available yet, it will wait
    // until the data was received. In particular, it is highly
    // discouraged to perform random data access.
    //
    // This method is thread-safe and can be invoked from multiple
    // threads at the same time.
    //
    // The returned chunk will have the exact size `size`, except
    // when requesting data past the end of the stream, in which
    // case the buffer `[start; eof)` will be returned. If the
    // initial offset `start` is past the end of stream, then an
    // empty Buffer will be returned.
    //
    virtual Buffer getChunk(size_t start, size_t size) = 0;

    // Call this method to inform the BufferedStream that data
    // at offsets `[0; upTo)` will no longer be needed. This will
    // allow some space to be freed (potentially).
    virtual void releaseChunk(size_t upTo) = 0;

    // Read data from the source stream.
    // This method will be executed from a dedicated thread, and the
    // implementation is allowed to be long-running.
    virtual void stream() = 0;

    // Stream API: return a chunk of the most appropriate size
    // Buffer readChunk(size_t requestedSize) override;
};




}}  // namespace dt::read2
#endif
