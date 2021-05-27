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
#include "utils/assert.h"
namespace dt {
namespace read2 {



//------------------------------------------------------------------------------
// BufferedStream_Buffer
//------------------------------------------------------------------------------

/**
  * Buffered stream class that wraps a regular Buffer.
  */
class BufferedStream_Buffer : public BufferedStream {
  private:
    Buffer buffer_;
    bool doneReading_;
    size_t : 56;

  public:
    BufferedStream_Buffer(Buffer&& buf)
      : buffer_(std::move(buf)),
        doneReading_(false) {}


    Buffer getChunk(size_t start, size_t size) override {
      size_t n = buffer_.size();
      if (start > n) size = 0;
      else if (start + size > n) size = n - start;
      return Buffer::view(buffer_, size, start);
    }

    void releaseChunk(size_t) override {}

    Buffer readNextChunk(size_t) override {
      if (doneReading_) {
        return Buffer();
      }
      else {
        doneReading_ = true;
        return buffer_;
      }
    }

    void reset() override {
      doneReading_ = false;
    }
};




//------------------------------------------------------------------------------
// BufferedStream_Stream
//------------------------------------------------------------------------------
#undef EOF

class BufferedStream_Stream : public BufferedStream {
  private:
    static constexpr size_t EOF = size_t(-1);
    struct Piece {
      size_t offset0;
      size_t offset1;
      Buffer buffer;
    };
    std::unique_ptr<Stream> stream_;
    std::deque<Piece> pieces_;
    size_t iterationIndex_;
    // Number of bytes read from the stream so far. When the stream reaches
    // end-of-file, this value will be set to `EOF`.
    size_t piecesNBytes_;
    std::mutex piecesMutex_;
    std::mutex streamMutex_;

  public:
    BufferedStream_Stream(std::unique_ptr<Stream>&& stream)
      : stream_(std::move(stream)),
        iterationIndex_(0),
        piecesNBytes_(0) {}


    Buffer getChunk(size_t start, size_t size) override {
      xassert(size > 0);
      while (true) {
        std::vector<Buffer> fragments;
        size_t remainingSize = size;
        size_t piecesNBytes;
        {
          // pieces_ array must be read under the protection of a mutex because
          // it is modified in `stream()` and `releaseChunk()`, which could be
          // invoked from other threads.
          std::lock_guard<std::mutex> lock(piecesMutex_);
          xassert(pieces_.empty() || start >= pieces_.front().offset0);
          piecesNBytes = piecesNBytes_;
          for (const auto& piece : pieces_) {
            const size_t offset0 = piece.offset0;
            const size_t offset1 = piece.offset1;
            if (start >= offset1) continue;
            size_t fragmentStart = (start > offset0)? start - offset0 : 0;
            size_t fragmentSize = std::min(remainingSize,
                                           offset1 - offset0 - fragmentStart);
            fragments.push_back(
                Buffer::view(piece.buffer, fragmentSize, fragmentStart));
            remainingSize -= fragmentSize;
            if (remainingSize == 0) break;
          }
        }  // `piecesMutex_` unlocked
        if (remainingSize == 0 || piecesNBytes == EOF) {
          return concatenateBuffers(fragments);
        }
        // otherwise, not all required pieces have been read yet -- need to
        // request additional data.
        {
          std::unique_lock<std::mutex> lock1(streamMutex_);
          // If these are not equal, it means some other thread has already read
          // the next piece of data while we were waiting for the lock. If this
          // is the case, we simply continue the main loop in order to check if
          // this new piece of data will satisfy our needs.
          if (piecesNBytes == piecesNBytes_) {
            readChunkUnchecked(std::max(remainingSize, size_t(1<<20)));
          }
        }
      }
    }

    void releaseChunk(size_t upTo) override {
      xassert(iterationIndex_ == 0);
      std::lock_guard<std::mutex> lock(piecesMutex_);
      for (const auto& piece : pieces_) {
        if (piece.offset1 > upTo) break;
        pieces_.pop_front();
      }
    }

    Buffer readNextChunk(size_t requestedSize) override {
      xassert(pieces_.empty() || pieces_.front().offset0 == 0);
      while (true) {
        if (iterationIndex_ < pieces_.size()) {
          const auto& piece = pieces_[iterationIndex_];
          iterationIndex_++;
          return piece.buffer;
        }
        if (piecesNBytes_ == EOF) {
          return Buffer();
        }
        readChunkUnchecked(requestedSize);
      }
    }

    void reset() override {
      iterationIndex_ = 0;
    }


  private:
    Buffer concatenateBuffers(const std::vector<Buffer>& buffers) {
      if (buffers.size() == 0) {
        return Buffer();
      }
      if (buffers.size() == 1) {
        return buffers[0];
      }
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

    void readChunkUnchecked(size_t requestedSize) {
      Buffer chunk = stream_->readNextChunk(requestedSize);
      size_t size = chunk.size();

      std::lock_guard<std::mutex> lock(piecesMutex_);
      if (size == 0) {
        piecesNBytes_ = EOF;
      } else {
        pieces_.push_back({ piecesNBytes_, piecesNBytes_ + size, chunk });
        piecesNBytes_ += size;
      }
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
