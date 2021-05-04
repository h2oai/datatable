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
#ifndef dt_READ2_STREAM_h
#define dt_READ2_STREAM_h
#include "read2/_declarations.h"
#include "python/obj.h"
namespace dt {
namespace read2 {


class Stream {
  public:
    virtual ~Stream();

    // Read the next chunk of data from the stream, and return to
    // the caller as a Buffer object. The `requestedSize` is a hint,
    // not a requirement. The implementation is allowed to return
    // either more or less data than requested.
    //
    // When the stream reaches its end, this function will return an
    // empty Buffer (0 size). It must not return empty Buffer if
    // there is still data to be read in the stream.
    //
    virtual Buffer readChunk(size_t requestedSize) = 0;
};




//------------------------------------------------------------------------------
// Implementations
//------------------------------------------------------------------------------

class Stream_Filelike : public Stream {
  private:
    py::oobj pyReadFn_;

  public:
    Stream_Filelike(py::robj src);

    Buffer readChunk(size_t requestedSize) override;
};



// provisional, NYI
class Stream_Encoding : public Stream {
  private:
    std::unique_ptr<Stream> upstream_;
  public:
    Buffer readChunk(size_t requestedSize) override;
};


// NYI
class Stream_LineProcessor : public Stream {
};



}}  // dt::read2
#endif
