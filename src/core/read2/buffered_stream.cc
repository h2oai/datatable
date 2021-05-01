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
#include <deque>
#include "read2/buffered_stream.h"
#include "read2/stream.h"
namespace dt {
namespace read2 {


//------------------------------------------------------------------------------
// BufferedStream_Buffer
//------------------------------------------------------------------------------

class BufferedStream_Buffer : public BufferedStream {
  private:
    Buffer buffer_;

  public:
    BufferedStream_Buffer(Buffer&& buf)
      : buffer_(std::move(buf)) {}


    Buffer getChunk(size_t start, size_t size) override {
      size_t n = buffer_.size();
      if (start > n) size = 0;
      else if (start + size > n) size = n - start;
      return Buffer::view(buffer_, size, start);
    }

    void stream() override {}
    void release(size_t) override {}
};




//------------------------------------------------------------------------------
// BufferedStream_Stream
//------------------------------------------------------------------------------

class BufferedStream_Stream : public BufferedStream {
  private:
    struct Piece {
      size_t offset0;
      size_t offset1;
      Buffer buffer;
    };
    std::unique_ptr<Stream> stream_;
    std::deque<Piece> pieces_;
    size_t nPiecesRead_;
    std::mutex mutex_;
    std::condition_variable cv_;

  public:
    BufferedStream_Stream(std::unique_ptr<Stream>&& stream)
      : stream_(std::move(stream)),
        nPiecesRead_(0) {}


    Buffer getChunk(size_t start, size_t size) override {
      while (true) {
        std::vector<Buffer> fragments;
        size_t remainingSize = size;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          for (const auto& piece : pieces_) {
            const size_t pieceStart = piece.offset0;
            const size_t pieceEnd = piece.offset1;
            if (pieceEnd <= start) continue;
            size_t fragmentStart = (pieceStart <= start)? start - pieceStart : 0;
            size_t fragmentSize = std::min(remainingSize, pieceEnd - pieceStart);
            fragments.push_back(
                Buffer::view(piece.buffer, fragmentSize, fragmentStart));
            remainingSize -= fragmentSize;
            if (remainingSize == 0) break;
          }
        }  // mutex released
        if (remainingSize == 0) {  // all memory pieces are available
          if (fragments.size() == 1) {
            return fragments.front();
          } else {
            return concatenateBuffers(fragments);
          }
        }
        // otherwise, not all required pieces have been read yet -- need to
        // wait until more data becomes available.
        {
          std::unique_lock<std::mutex> lock(mutex_);
          size_t n = nPiecesRead_;
          cv_.wait(lock, [&]{ return nPiecesRead_ > n; });
        }
      }
    }

    void stream() override {
      while (true) {
        auto buffer = stream_->readChunk(1024*1024);
        auto size = buffer.size();
        if (!size) break;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          size_t offset0 = pieces_.empty()? 0 : pieces_.back().offset1;
          pieces_.push_back(Piece{ offset0, offset0 + size, buffer });
          nPiecesRead_++;
        }
        cv_.notify_all();
      }
    }

    void release(size_t upTo) override {
      std::lock_guard<std::mutex> lock(mutex_);
      for (const auto& piece : pieces_) {
        if (piece.offset1 > upTo) break;
        pieces_.pop_front();
      }
    }


  private:
    Buffer concatenateBuffers(const std::vector<Buffer>& buffers) {
      xassert(buffers.size() >= 2);
      size_t total = 0;
      for (const auto& buf : buffers) {
        total += buf.size();
      }
      Buffer out = Buffer::mem(total);
      char* outPtr = static_cast<char*>(out.xptr());
      for (const auto& buf : buffers) {
        std::memcpy(outPtr, buf.rptr(), buf.size());
        outPtr += buf.size();
      }
      return out;
    }
};




//------------------------------------------------------------------------------
// BufferedStream
//------------------------------------------------------------------------------
using BufferedStreamPtr = std::unique_ptr<BufferedStream>;

BufferedStream::~BufferedStream() {}


BufferedStreamPtr BufferedStream::fromBuffer(Buffer buf) {
  return BufferedStreamPtr(new BufferedStream_Buffer(std::move(buf)));
}


BufferedStreamPtr BufferedStream::fromStream(std::unique_ptr<Stream>&& stream) {
  return BufferedStreamPtr(new BufferedStream_Stream(std::move(stream)));
}




}}  // namespace dt::read2
