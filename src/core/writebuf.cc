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
#include <algorithm>         // std::min
#include <cstring>           // std::memcpy
#include <errno.h>           // errno
#include "utils/alloc.h"     // dt::realloc
#include "utils/assert.h"
#include "utils/macros.h"
#include "utils/misc.h"
#include "buffer.h"
#include "writebuf.h"

#if DT_OS_WINDOWS
  #include <io.h>            // _write
  #include "lib/mman/mman.h" // mmap, munmap

  // The POSIX `write()` function is deprecated on Windows,
  // so we use the ISO C++ conformant `_write()` instead.
  #define WRITE ::_write
#else
  #include <sys/mman.h>      // mmap
  #include <unistd.h>        // write
  #define WRITE ::write
#endif



//==============================================================================
// WritableBuffer
//==============================================================================

WritableBuffer::WritableBuffer()
  : bytes_written_(0) {}

WritableBuffer::~WritableBuffer() {}


// We use different strategy on MacOS than on other operating systems because
// Macs' default file system HFS does not support sparse files, which means
// that trying to create an empty file of size `size` (so that it can be later
// memory-mapped) would cause the operating system to physically write that
// many zeros into the file. This effectively means that the file will be
// written twice, which results in degraded performance. In my experiments,
// switching to FileWritableBuffer on MacOS improved the overall time of writing
// a CSV by a factor of 2 (on a 4GB file).
//
std::unique_ptr<WritableBuffer> WritableBuffer::create_target(
    const std::string& path, size_t size, WritableBuffer::Strategy strategy,
    bool append)
{
  WritableBuffer* res = nullptr;
  if (path.empty()) {
    res = new MemoryWritableBuffer(size);
  } else {
    if (strategy == Strategy::Auto) {
      #if DT_OS_MACOS
        strategy = Strategy::Write;
      #else
        strategy = Strategy::Mmap;
      #endif
    }
    if (strategy == Strategy::Write) {
      res = new FileWritableBuffer(path, append);
    }
    if (strategy == Strategy::Mmap) {
      res = new MmapWritableBuffer(path, size, append);
    }
  }
  return std::unique_ptr<WritableBuffer>(res);
}


size_t WritableBuffer::size() const {
  return bytes_written_;
}




//==============================================================================
// FileWritableBuffer
//==============================================================================

FileWritableBuffer::FileWritableBuffer(const std::string& path, bool append) {
  file_ = new File(path, append? File::APPEND
                               : File::OVERWRITE);
}

FileWritableBuffer::~FileWritableBuffer() {
  delete file_;
}


size_t FileWritableBuffer::prepare_write(size_t src_size, const void* src) {
  constexpr size_t CHUNK_SIZE = 1 << 30;
  size_t pos = bytes_written_;
  if (!src_size) return pos;
  XAssert(src);

  // On MacOS, it is impossible to write more than 2GB of data at once; on
  // Unix, the limit is 0x7ffff000 bytes.
  // Thus, we avoid attempting to write more than 1GB of data at a time,
  // splitting the data into chunks if necessary (#1387).
  //
  // See: https://linux.die.net/man/2/write
  //
  int fd = file_->descriptor();
  int attempts_remaining = 5;
  size_t written_to_file = 0;
  while (written_to_file < src_size) {
    size_t bytes_to_write = std::min(src_size - written_to_file, CHUNK_SIZE);
    auto buf = static_cast<const char*>(src) + written_to_file;
    auto r = WRITE(fd, buf, static_cast<unsigned int>(bytes_to_write));
    if (r < 0) {
      throw IOError() << "Cannot write to file: " << Errno
          << " (started at offset " << pos
          << ", written " << written_to_file
          << " out of " << src_size << " bytes)";
    }
    if (r == 0) {
      if (attempts_remaining--) continue;  // Retry several times
      throw IOError() << "Output to file truncated: "
          << written_to_file << " out of " << src_size << " bytes written";
    }
    // Normally, `r` contains the number of bytes written to file. This could
    // be less than the amount requested.
    // This could happen if: (a) there is insufficient space on the target
    // physical medium, (b) RLIMIT_FSIZE resource limit is encountered,
    // (c) the call was interrupted by a signal handler before all data was
    // written.
    written_to_file += static_cast<size_t>(r);
  }
  XAssert(written_to_file == src_size);
  bytes_written_ += written_to_file;
  return pos;
}


void FileWritableBuffer::write_at(size_t, size_t, const void*) {
  // Do nothing. FileWritableBuffer does all the writing at the "prepare_write"
  // stage, because it is unable to write from multiple threads at the same
  // time (which would have required keeping multiple `fd`s on the same file
  // open, and then each thread seek-and-write to its own location.
  // Microbenchmarks show that this ends up being slower than the simple
  // single-threaded writing. Additionally, on some systems this may result in
  // lost content, if the OS decides to fill the gap with 0s when another
  // thread is writing there).
}


void FileWritableBuffer::finalize() {
  delete file_;
  file_ = nullptr;
}




//==============================================================================
// ThreadsafeWritableBuffer
//==============================================================================

ThreadsafeWritableBuffer::ThreadsafeWritableBuffer()
  : data_(nullptr), allocsize_(0) {}


size_t ThreadsafeWritableBuffer::prepare_write(size_t n, const void*) {
  size_t pos = bytes_written_;
  size_t nbw = pos + n;

  if (nbw > allocsize_) {
    dt::shared_lock<dt::shared_mutex> lock(shmutex_, /* exclusive = */ true);
    size_t newsize = nbw * 2;
    this->realloc(newsize);
    xassert(allocsize_ >= newsize);
  }

  bytes_written_ = nbw;
  return pos;
}


void ThreadsafeWritableBuffer::write_at(size_t pos, size_t n, const void* src) {
  // When n==0, the `data_` pointer may remain unallocated, and it
  // is invalid to `memcpy` 0 bytes into a null pointer.
  if (n == 0) return;
  if (pos + n > allocsize_) {
    throw RuntimeError() << "Attempt to write at pos=" << pos << " chunk of "
      "length " << n << ", however the buffer is allocated for " << allocsize_
      << " bytes only";
  }
  dt::shared_lock<dt::shared_mutex> lock(shmutex_, /* exclusive = */ false);
  XAssert(src && data_);  // data_ should be accessed under the mutex
  char* target = static_cast<char*>(data_) + pos;
  std::memcpy(target, src, n);
}


void ThreadsafeWritableBuffer::finalize() {
  dt::shared_lock<dt::shared_mutex> lock(shmutex_, /* exclusive = */ true);
  this->realloc(bytes_written_);
}



//==============================================================================
// Writer (helper for MemoryWritableBuffer)
//==============================================================================

MemoryWritableBuffer::Writer::Writer(MemoryWritableBuffer* parent,
                                     size_t start, size_t end)
  : mbuf_(parent),
    pos_start_(start),
    pos_end_(end)
{
  XAssert(mbuf_ && pos_end_ <= mbuf_->allocsize_);
  mbuf_->shmutex_.lock_shared();
}

MemoryWritableBuffer::Writer::Writer(Writer&& o)
  : mbuf_(o.mbuf_),
    pos_start_(o.pos_start_),
    pos_end_(o.pos_end_)
{
  o.mbuf_ = nullptr;
}


MemoryWritableBuffer::Writer::~Writer() {
  if (mbuf_) mbuf_->shmutex_.unlock_shared();
}


void MemoryWritableBuffer::Writer::write_at(size_t pos,
                                            const char* src, size_t len)
{
  if (!len) return;
  xassert(pos >= pos_start_ && pos + len <= pos_end_);
  xassert(mbuf_->data_);
  std::memcpy(static_cast<char*>(mbuf_->data_) + pos, src, len);
}




//==============================================================================
// MemoryWritableBuffer
//==============================================================================

MemoryWritableBuffer::MemoryWritableBuffer(size_t size)
  : ThreadsafeWritableBuffer()
{
  this->realloc(size);
}


MemoryWritableBuffer::~MemoryWritableBuffer() {
  dt::free(data_);
}


void MemoryWritableBuffer::realloc(size_t newsize) {
  data_ = dt::realloc(data_, newsize);
  allocsize_ = newsize;
}


Buffer MemoryWritableBuffer::get_mbuf() {
  Buffer buf = Buffer::acquire(data_, allocsize_);
  data_ = nullptr;
  allocsize_ = 0;
  return buf;
}


// This method leaves `data_` intact; and it will be freed when the destructor
// is invoked.
std::string MemoryWritableBuffer::get_string() {
  return std::string(static_cast<char*>(data_), allocsize_);
}


void MemoryWritableBuffer::clear() {
  bytes_written_ = 0;
}

void* MemoryWritableBuffer::data() const {
  return data_;
}


MemoryWritableBuffer::Writer MemoryWritableBuffer::writer(size_t start, size_t end) {
  return MemoryWritableBuffer::Writer(this, start, end);
}




//==============================================================================
// MmapWritableBuffer
//==============================================================================

MmapWritableBuffer::MmapWritableBuffer(const std::string& path, size_t size,
                                       bool append)
  : ThreadsafeWritableBuffer(), filename_(path)
{
  File file(path, File::CREATE);
  if (append) {
    size_t filesize = file.size();
    size += filesize;
    bytes_written_ = filesize;
  }
  if (size) {
    file.resize(size);
    allocsize_ = size;
    map(file.descriptor(), size);
  }
}


MmapWritableBuffer::~MmapWritableBuffer() {
  try {
    unmap();
  } catch (const std::exception& e) {
    printf("%s\n", e.what());
  }
}


void MmapWritableBuffer::map(int fd, size_t size) {
  if (data_) {
    throw ValueError() << "data_ is not null";
  }
  if (size == 0) {
    allocsize_ = 0;
    return;
  }
  data_ = mmap(nullptr, size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
  if (data_ == MAP_FAILED) {
    data_ = nullptr;
    throw RuntimeError() << "Memory map failed for file " << filename_
                         << " of size " << size << Errno;
  }
  allocsize_ = size;
}


void MmapWritableBuffer::unmap() {
  if (!data_) return;
  // Do not short-circuit: both `msync` and `munmap` must run
  int ret = msync(data_, allocsize_, MS_ASYNC) |
            munmap(data_, allocsize_);
  if (ret) {
    throw IOError() << "Error unmapping the view of file "
        << filename_ << " (" << data_ << "..+" << allocsize_
        << "): " << Errno;
  }
  data_ = nullptr;
}


void MmapWritableBuffer::realloc(size_t newsize) {
  unmap();
  File file(filename_, File::READWRITE);
  file.resize(newsize);
  map(file.descriptor(), newsize);
}


// Similar to realloc(), but does not re-map the file
void MmapWritableBuffer::finalize() {
  dt::shared_lock<dt::shared_mutex> lock(shmutex_, /* exclusive = */ true);
  unmap();
  File file(filename_, File::READWRITE);
  file.resize(bytes_written_);
}
