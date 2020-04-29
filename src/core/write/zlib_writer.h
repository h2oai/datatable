//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#ifndef dt_WRITE_ZLIB_WRITER_h
#define dt_WRITE_ZLIB_WRITER_h
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "types.h"
#include "lib/zlib/zlib.h"
namespace dt {
namespace write {



class zlib_writer {
  private:
    zlib::z_stream stream;
    char* buffer;
    size_t buffer_capacity;

  public:
    zlib_writer() {
      buffer = nullptr;
      buffer_capacity = 0;
      using z_stream = zlib::z_stream;  // for deflateInit2() macro
      stream.zalloc = nullptr;
      stream.zfree = nullptr;
      stream.opaque = nullptr;
      stream.data_type = Z_TEXT;
      // TODO: expose <compression_level> as a global option
      int r = zlib::deflateInit2(&stream,
                                 Z_DEFAULT_COMPRESSION,  // compression level
                                 Z_DEFLATED,  // method
                                 10 + 16,  // windowBits, +16 for gzip headers
                                 8,  // memLevel (default = 8)
                                 Z_DEFAULT_STRATEGY);  // strategy
      if (r != Z_OK) {
        throw RuntimeError() << "Error " << r << " in zlib::deflateInit2()";
      }
      stream.next_in = nullptr;
    }
    zlib_writer(const zlib_writer&) = delete;
    zlib_writer(zlib_writer&&) = delete;

    ~zlib_writer() {
      delete[] buffer;
      // deflateEnd(z_streamp)
      //  | All dynamically allocated data structures for this stream are freed.
      //  | This function discards any unprocessed input and does not flush any
      //  | pending output.
      int r = zlib::deflateEnd(&stream);
      (void)r;
      wassert(r == Z_OK);
    }

    /**
     * compress(inout)
     *
     * Compress data in the input buffer `inout`, and then update this variable
     * with the location of the compressed data. The character data buffer
     * returned is still owned by this class, the caller should not attempt to
     * free it. The returned buffer remains valid until the next call to
     *`compress()`, or until this object is destroyed.
     *
     * The returned data is wrapped in GZIP headers, so that it can be readily
     * written into a .gz file. This is based on the fact that the GZIP format
     * allows multiple "members" to be written one after another in the output
     * .gz file (see RFC-1952 §2.2). Thus, it is valid to split the input into
     * multiple chunks, compress each chunk individually, and then write all
     * these compressed chunks into a single output file without any additional
     * effort.
     *
     * The overhead of each GZIP header is 10 + 8 bytes surrounding the
     * compressed data chunk.
     *
     */
    void compress(CString& inout) {
      size_t input_size = static_cast<size_t>(inout.size);
      if (input_size != static_cast<zlib::uLong>(input_size)) {
        throw RuntimeError() << "Cannot compress chunk of size " << input_size;
      }
      size_t out_size = zlib::deflateBound(&stream, input_size);  // estimated
      ensure_buffer_capacity(out_size);

      reset_buffer();
      stream.next_in = reinterpret_cast<zlib::Bytef*>(const_cast<char*>(inout.ch));
      stream.avail_in = static_cast<zlib::uInt>(input_size);
      stream.next_out = reinterpret_cast<zlib::Bytef*>(buffer);
      stream.avail_out = static_cast<zlib::uInt>(buffer_capacity);

      // deflate(z_streamp, int fliush)
      //   | If the parameter flush is set to Z_FINISH, pending input is processed,
      //   | pending output is flushed and deflate returns with Z_STREAM_END if
      //   | there was enough output space. If deflate returns with Z_OK or
      //   | Z_BUF_ERROR, this function must be called again with Z_FINISH and more
      //   | output space (updated avail_out) but no more input data, until it
      //   | returns with Z_STREAM_END or an error. After deflate has returned
      //   | Z_STREAM_END, the only possible operations on the stream are
      //   | deflateReset or deflateEnd.
      int r = zlib::deflate(&stream, Z_FINISH);
      if (r != Z_STREAM_END) {
        throw RuntimeError() << "Error " << r << " in zlib::deflate(Z_FINISH)";
      }
      xassert(stream.avail_in == 0);
      xassert(stream.total_in == input_size);
      inout.ch = buffer;
      inout.size = static_cast<int64_t>(stream.total_out);
    }


  private:
    void ensure_buffer_capacity(size_t sz) {
      if (buffer_capacity >= sz) return;
      delete[] buffer;
      buffer = new char[sz];
      buffer_capacity = sz;
    }

    void reset_buffer() {
      if (!stream.next_in) return;
      int r = zlib::deflateReset(&stream);
      (void)r;
      wassert(r == Z_OK);
    }
};



}}  // namespace dt::write
#endif
