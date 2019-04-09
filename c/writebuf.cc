//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <cstring>     // std::memcpy
#include <errno.h>     // errno
#include <sys/mman.h>  // mmap
#include <unistd.h>    // write
#include "utils/alloc.h"   // dt::realloc
#include "utils/assert.h"
#include "utils/misc.h"
#include "datatablemodule.h"
#include "memrange.h"
#include "writebuf.h"



//==============================================================================
// WritableBuffer
//==============================================================================

// Do not define ~WritableBuffer() inline in writebuf.h, because it triggers
// the -Wweak-tables warning.
WritableBuffer::WritableBuffer(): bytes_written(0) {}
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
    const std::string& path, size_t size, WritableBuffer::Strategy strategy)
{
  WritableBuffer* res = nullptr;
  if (path.empty()) {
    res = new MemoryWritableBuffer(size);
  } else {
    if (strategy == Strategy::Auto) {
      #ifdef __APPLE__
        strategy = Strategy::Write;
      #else
        strategy = Strategy::Mmap;
      #endif
    }
    if (strategy == Strategy::Write) {
      res = new FileWritableBuffer(path);
    }
    if (strategy == Strategy::Mmap) {
      res = new MmapWritableBuffer(path, size);
    }
  }
  return std::unique_ptr<WritableBuffer>(res);
}



//==============================================================================
// FileWritableBuffer
//==============================================================================

FileWritableBuffer::FileWritableBuffer(const std::string& path) {
  file = new File(path, File::OVERWRITE);
  TRACK(this, sizeof(*this), "FileWritableBuffer");
}

FileWritableBuffer::~FileWritableBuffer() {
  delete file;
  UNTRACK(this);
}


size_t FileWritableBuffer::prep_write(size_t src_size, const void* src)
{
  constexpr size_t CHUNK_SIZE = 1 << 30;
  size_t pos = bytes_written;
  if (!src_size) return pos;

  // On MacOS, it is impossible to write more than 2GB of data at once; on
  // Unix, the limit is 0x7ffff000 bytes.
  // Thus, we avoid attempting to write more than 1GB of data at a time,
  // splitting the data into chunks if necessary (#1387).
  //
  // See: https://linux.die.net/man/2/write
  //
  int fd = file->descriptor();
  size_t written_to_file = 0;
  while (written_to_file < src_size) {
    size_t count = std::min(src_size, CHUNK_SIZE);
    const void* buf = static_cast<const char*>(src) + written_to_file;
    ssize_t r = ::write(fd, buf, count);
    if (r == -1) {
      throw RuntimeError() << "Cannot write to file: " << Errno
          << " (bytes already written: " << bytes_written << ")";
    }
    if (r == 0) {
      throw RuntimeError() << "Output to file truncated: "
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
  xassert(written_to_file == src_size);
  bytes_written += written_to_file;
  return pos;
}


void FileWritableBuffer::write_at(size_t, size_t, const void*)
{
  // Do nothing. FileWritableBuffer does all the writing at the "prep_write"
  // stage, because it is unable to write from multiple threads at the same
  // time (which would have required keeping multiple `fd`s on the same file
  // open, and then each thread seek-and-write to its own location.
  // Microbenchmarks show that this ends up being slower than the simple
  // single-threaded writing. Additionally, on some systems this may result in
  // lost content, if the OS decides to fill the gap with 0s when another
  // thread is writing there).
}


void FileWritableBuffer::finalize() {
  delete file;
  file = nullptr;
}



//==============================================================================
// ThreadsafeWritableBuffer
//==============================================================================

ThreadsafeWritableBuffer::ThreadsafeWritableBuffer()
  : buffer(nullptr), allocsize(0) {}


ThreadsafeWritableBuffer::~ThreadsafeWritableBuffer() {}


size_t ThreadsafeWritableBuffer::prep_write(size_t n, const void*) {
  size_t pos = bytes_written;
  size_t nbw = pos + n;

  if (nbw > allocsize) {
    dt::shared_lock<dt::shared_mutex> lock(shmutex, /* exclusive = */ true);
    size_t newsize = nbw * 2;
    this->realloc(newsize);
    xassert(allocsize >= newsize);
  }

  bytes_written = nbw;
  return pos;
}


void ThreadsafeWritableBuffer::write_at(size_t pos, size_t n, const void* src) {
  if (pos + n > allocsize) {
    throw RuntimeError() << "Attempt to write at pos=" << pos << " chunk of "
      "length " << n << ", however the buffer is allocated for " << allocsize
      << " bytes only";
  }
  dt::shared_lock<dt::shared_mutex> lock(shmutex, /* exclusive = */ false);
  char* target = static_cast<char*>(buffer) + pos;
  std::memcpy(target, src, n);
}


void ThreadsafeWritableBuffer::finalize() {
  dt::shared_lock<dt::shared_mutex> lock(shmutex, /* exclusive = */ true);
  this->realloc(bytes_written);
}



//==============================================================================
// MemoryWritableBuffer
//==============================================================================

MemoryWritableBuffer::MemoryWritableBuffer(size_t size)
  : ThreadsafeWritableBuffer()
{
  this->realloc(size);
  TRACK(this, sizeof(*this), "MemoryWritableBuffer");
}


MemoryWritableBuffer::~MemoryWritableBuffer() {
  dt::free(buffer);
  UNTRACK(this);
}


void MemoryWritableBuffer::realloc(size_t newsize)
{
  buffer = dt::realloc(buffer, newsize);
  allocsize = newsize;
}


void* MemoryWritableBuffer::get_cptr()
{
  void* buf = buffer;
  buffer = nullptr;
  allocsize = 0;
  return buf;
}


MemoryRange MemoryWritableBuffer::get_mbuf() {
  size_t size = allocsize;
  void* ptr = get_cptr();
  return MemoryRange::acquire(ptr, size);
}


// This method leaves `buffer` intact; and it will be freed when the destructor
// is invoked.
std::string MemoryWritableBuffer::get_string() {
  return std::string(static_cast<char*>(buffer), allocsize);
}



//==============================================================================
// MmapWritableBuffer
//==============================================================================

MmapWritableBuffer::MmapWritableBuffer(const std::string& path, size_t size)
  : ThreadsafeWritableBuffer(), filename(path)
{
  File file(path, File::CREATE);
  if (size) {
    file.resize(size);
    allocsize = size;
    map(file.descriptor(), size);
  }
  TRACK(this, sizeof(*this), "MmapWritableBuffer");
}


MmapWritableBuffer::~MmapWritableBuffer() {
  unmap();
  UNTRACK(this);
}


void MmapWritableBuffer::map(int fd, size_t size) {
  if (buffer) {
    throw ValueError() << "buffer is not null";
  }
  if (size == 0) {
    allocsize = 0;
    return;
  }
  buffer = mmap(nullptr, size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
  if (buffer == MAP_FAILED) {
    buffer = nullptr;
    throw RuntimeError() << "Memory map failed for file " << filename
                         << " of size " << size << Errno;
  }
  allocsize = size;
}


void MmapWritableBuffer::unmap() {
  if (!buffer) return;
  int ret = munmap(buffer, allocsize);
  if (ret) {
    printf("Error unmapping the view of file %s (%p..+%zu): [errno %d] %s",
           filename.c_str(), buffer, allocsize, errno, strerror(errno));
  }
  buffer = nullptr;
}


void MmapWritableBuffer::realloc(size_t newsize)
{
  unmap();
  File file(filename, File::READWRITE);
  file.resize(newsize);
  map(file.descriptor(), newsize);
}
