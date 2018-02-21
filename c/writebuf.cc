//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "writebuf.h"
#include <cstdlib>     // std::realloc
#include <cstring>     // std::memcpy
#include <errno.h>     // errno
#include <sys/mman.h>  // mmap
#include <unistd.h>    // write
#include "memorybuf.h"
#include "utils/assert.h"
#include "utils/omp.h"
#include "utils.h"



//==============================================================================
// WritableBuffer
//==============================================================================


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
}

FileWritableBuffer::~FileWritableBuffer() {
  delete file;
}


size_t FileWritableBuffer::prep_write(size_t size, const void* src)
{
  // See https://linux.die.net/man/2/write
  ssize_t r = ::write(file->descriptor(), src, size);

  if (r == -1) {
    throw RuntimeError() << "Cannot write to file: " << Errno
                         << " (bytes already written: " << bytes_written << ")";
  }
  if (r < static_cast<ssize_t>(size)) {
    // This could happen if: (a) there is insufficient space on the target
    // physical medium, (b) RLIMIT_FSIZE resource limit is encountered,
    // (c) the call was interrupted by a signal handler before all data was
    // written.
    throw RuntimeError() << "Output to file truncated: " << r << " out of "
                         << size << " bytes written";
  }
  bytes_written += size;
  return bytes_written;
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


void FileWritableBuffer::finalize()
{
  delete file;
  file = nullptr;
}



//==============================================================================
// ThreadsafeWritableBuffer
//==============================================================================

ThreadsafeWritableBuffer::ThreadsafeWritableBuffer()
  : buffer(nullptr), allocsize(0), nlocks(0) {}


ThreadsafeWritableBuffer::~ThreadsafeWritableBuffer() {}


size_t ThreadsafeWritableBuffer::prep_write(size_t n, const void*)
{
  size_t pos = bytes_written;
  bytes_written += n;

  // In the rare case when we need to reallocate the underlying buffer (because
  // more space is needed than originally anticipated), this will block until
  // all other threads finish writing their chunks, and only then proceed with
  // the reallocation. Otherwise, reallocating the memory when some other thread
  // is writing into it leads to very-hard-to-debug crashes...
  while (bytes_written > allocsize) {
    size_t newsize = bytes_written * 2;
    int lockv = 1;
    // (1) wait until no other process is writing into the buffer
    while (lockv) {
      #pragma omp atomic read
      lockv = nlocks;
    }
    // (2) make `numuses` negative, indicating that no other thread may
    //     initiate a memcopy operation for now.
    #pragma omp atomic capture
    { lockv = nlocks; nlocks -= 1000000; }
    // (3) The only case when `lockv != 0` is if another thread started memcopy
    //     operation in-between steps (1) and (2) above. In that case we restore
    //     the previous value of `numuses` and repeat the loop.
    //     Otherwise (and it is the most common case) we reallocate the buffer
    //     and only then restore the `numuses` variable.
    if (lockv == 0) {
      this->realloc(newsize);
    }
    #pragma omp atomic update
    nlocks += 1000000;
  }

  return pos;
}


void ThreadsafeWritableBuffer::write_at(size_t pos, size_t n, const void* src)
{
  assert(pos + n <= allocsize);
  bool done = false;
  while (!done) {
    int lockv;
    #pragma omp atomic capture
    lockv = nlocks++;
    if (lockv >= 0) {
      void* target = static_cast<void*>(static_cast<char*>(buffer) + pos);
      std::memcpy(target, src, n);
      done = true;
    }
    #pragma omp atomic update
    nlocks--;
  }
}


void ThreadsafeWritableBuffer::finalize()
{
  int lockv = 1;
  while (lockv) {
    #pragma omp atomic read
    lockv = nlocks;
  }
  while (allocsize > bytes_written) {
    lockv = 1;
    while (lockv) {
      #pragma omp atomic read
      lockv = nlocks;
    }
    #pragma omp atomic capture
    { lockv = nlocks; nlocks -= 1000000; }
    if (lockv == 0) {
      this->realloc(bytes_written);
    }
    #pragma omp atomic update
    nlocks += 1000000;
  }
}



//==============================================================================
// MemoryWritableBuffer
//==============================================================================

MemoryWritableBuffer::MemoryWritableBuffer(size_t size)
  : ThreadsafeWritableBuffer()
{
  if (size) {
    buffer = std::malloc(size);
    allocsize = size;
    if (!buffer) {
      throw MemoryError() << "Unable to allocate memory of size " << size;
    }
  }
}


MemoryWritableBuffer::~MemoryWritableBuffer()
{
  std::free(buffer);
}


void MemoryWritableBuffer::realloc(size_t newsize)
{
  if (newsize) {
    buffer = std::realloc(buffer, newsize);
    allocsize = newsize;
    if (!buffer) {
      throw MemoryError() << "Unable to allocate memory of size " << newsize;
    }
  } else {
    std::free(buffer);
    buffer = nullptr;
    allocsize = 0;
  }
}


void* MemoryWritableBuffer::get_cptr()
{
  void* buf = buffer;
  buffer = nullptr;
  allocsize = 0;
  return buf;
}


MemoryMemBuf* MemoryWritableBuffer::get_mbuf() {
  size_t size = allocsize;
  void* ptr = get_cptr();
  return new MemoryMemBuf(ptr, size);
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
}


MmapWritableBuffer::~MmapWritableBuffer() {
  unmap();
}


void MmapWritableBuffer::map(int fd, size_t size) {
  if (buffer) {
    throw ValueError() << "buffer is not null";
  }
  if (size == 0) {
    allocsize = 0;
    return;
  }
  buffer = mmap(NULL, size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
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
