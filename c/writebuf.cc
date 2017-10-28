//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "writebuf.h"
#include <errno.h>     // errno
#include <sys/mman.h>  // mmap
#include <unistd.h>    // write
#include "myomp.h"
#include "utils.h"



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

ThreadsafeWritableBuffer::ThreadsafeWritableBuffer(size_t size)
  : buffer(nullptr), allocsize(size), nlocks(0) {}


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
    int old = 0;
    // (1) wait until no other process is writing into the buffer
    while (nlocks > 0) {}
    // (2) make `numuses` negative, indicating that no other thread may
    //     initiate a memcopy operation for now.
    #pragma omp atomic capture
    { old = nlocks; nlocks -= 1000000; }
    // (3) The only case when `old != 0` is if another thread started memcopy
    //     operation in-between steps (1) and (2) above. In that case we restore
    //     the previous value of `numuses` and repeat the loop.
    //     Otherwise (and it is the most common case) we reallocate the buffer
    //     and only then restore the `numuses` variable.
    if (old == 0) {
      this->realloc(newsize);
    }
    #pragma omp atomic update
    nlocks += 1000000;
  }

  return pos;
}


void ThreadsafeWritableBuffer::write_at(size_t pos, size_t n, const void *src)
{
  int done = 0;
  while (!done) {
    int old;
    #pragma omp atomic capture
    old = nlocks++;
    if (old >= 0) {
      void *target = static_cast<void*>(static_cast<char*>(buffer) + pos);
      memcpy(target, src, n);
      done = 1;
    }
    #pragma omp atomic update
    nlocks--;
  }
}


void ThreadsafeWritableBuffer::finalize()
{
  while (nlocks > 0) {}
  while (allocsize > bytes_written) {
    while (nlocks > 0) {}
    int old = 0;
    #pragma omp atomic capture
    { old = nlocks; nlocks -= 1000000; }
    if (old == 0) {
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
  : ThreadsafeWritableBuffer(size)
{
  buffer = malloc(size);
  if (!buffer) {
    throw MemoryError() << "Unable to allocate memory buffer of size " << size;
  }
}


MemoryWritableBuffer::~MemoryWritableBuffer()
{
  free(buffer);
}


void MemoryWritableBuffer::realloc(size_t newsize)
{
  buffer = ::realloc(buffer, newsize);
  if (!buffer) {
    throw MemoryError() << "Unable to allocate memory of size " << newsize;
  }
  allocsize = newsize;
}


void* MemoryWritableBuffer::get()
{
  void *buf = buffer;
  buffer = nullptr;
  allocsize = 0;
  return buf;
}



//==============================================================================
// MmapWritableBuffer
//==============================================================================

MmapWritableBuffer::MmapWritableBuffer(const std::string& path, size_t size)
  : ThreadsafeWritableBuffer(size), filename(path)
{
  File file(path, File::CREATE);
  if (size) file.resize(size);
  map(file.descriptor(), size);
}


MmapWritableBuffer::~MmapWritableBuffer() {
  unmap();
}


void MmapWritableBuffer::map(int fd, size_t size) {
  if (buffer) {
    throw ValueError() << "buffer is not null";
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
