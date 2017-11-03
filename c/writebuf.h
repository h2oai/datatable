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
#ifndef dt_WRITEBUF_H
#define dt_WRITEBUF_H
#include <stdio.h>     // size_t
#include <string>      // std::string
#include "utils/file.h"



//==============================================================================

class WritableBuffer
{
protected:
  size_t bytes_written;

public:
  enum class Strategy : int8_t { Auto, Mmap, Write };

  WritableBuffer(): bytes_written(0) {}
  virtual ~WritableBuffer() {}

  /**
   * Factory method for instantiating one of the `WritableBuffer` derived
   * classes based on the strategy and the current platform.
   *
   * @param path
   *     Name of the file to write to. If empty, then we will be writing into
   *     the memory, and MemoryWritableBuffer is returned.
   *
   * @param size
   *     Expected size of the data to be written. This doesn't have to be the
   *     exact amount, however having a good estimate may improve efficiency.
   *     This may also have effect on the choice of the writing strategy.
   *
   * @param strategy
   *     Which subclass of `WritableBuffer` to construct. This could be `Auto`
   *     (choose the best subclass automatically), while all  other values
   *     force the choice of a particular subclass.
   */
  static std::unique_ptr<WritableBuffer> create_target(
    const std::string& path, size_t size, Strategy strategy = Strategy::Auto
  );

  /**
   * Return the current size of the writable buffer -- i.e. the amount of bytes
   * written into it. Note that this is different from the current buffer's
   * "capacity" (i.e. the size it was pre-allocated for).
   */
  size_t size() const { return bytes_written; }

  /**
   * Prepare to write buffer `src` of size `n`. This method is expected to be
   * called by no more than one thread at a time (for example from the OMP
   * "ordered" section). The value returned by this method should be passed to
   * the subsequent `write_at()` call.
   *
   * Implementations are encouraged to perform as little work as possible within
   * this method, and instead defer most writing to the `write_at()` method.
   * However in case when this is not possible, an implementation may actually
   * write out the provided buffer `src`.
   */
  virtual size_t prep_write(size_t n, const void *src) = 0;

  /**
   * Write buffer `src` of size `n` at the position `pos` (this position should
   * have previously been returned from `pre_write()`, which also ensured that
   * there is enough space to perform the writing).
   *
   * This call is safe to invoke from multiple threads simultaneously. It is
   * also allowed to call this method when another thread is performing
   * `prep_write()`.
   */
  virtual void write_at(size_t pos, size_t n, const void *src) = 0;

  /**
   * This method should be called when you're done writing to the buffer. It is
   * distinct from the destructor in that it is not expected to free any
   * resources, but rather make the object read-only.
   */
  virtual void finalize() = 0;

  /**
   * Simple helper method for writing into the buffer in single-threaded
   * context.
   */
  void write(size_t n, const void *src) {
    write_at(prep_write(n, src), n, src);
  }

  // Prevent copying / assignment for these objects
  WritableBuffer(const WritableBuffer&) = delete;
  WritableBuffer& operator=(const WritableBuffer&) = delete;
};



//==============================================================================

class FileWritableBuffer : public WritableBuffer
{
  File* file;

public:
  FileWritableBuffer(const std::string &path);
  virtual ~FileWritableBuffer();

  virtual size_t prep_write(size_t n, const void *src);
  virtual void write_at(size_t pos, size_t n, const void *src);
  virtual void finalize();
};



//==============================================================================

class ThreadsafeWritableBuffer : public WritableBuffer
{
protected:
  void*  buffer;
  size_t allocsize;
  volatile int nlocks;
  int    _unused;

  virtual void realloc(size_t newsize) = 0;

public:
  ThreadsafeWritableBuffer(size_t size);
  virtual ~ThreadsafeWritableBuffer();

  virtual size_t prep_write(size_t n, const void *src);
  virtual void write_at(size_t pos, size_t n, const void *src);
  virtual void finalize();
};



//==============================================================================

class MemoryWritableBuffer : public ThreadsafeWritableBuffer
{
public:
  MemoryWritableBuffer(size_t size);
  virtual ~MemoryWritableBuffer();

  /**
   * Return memory buffer that was written. This method may only be called
   * after `finalize()`. This class surrenders ownership of the buffer, and
   * it will be the responsibility of the caller to handle it.
   */
  void* get();

private:
  void realloc(size_t newsize) override;
};



//==============================================================================

class MmapWritableBuffer : public ThreadsafeWritableBuffer
{
  std::string filename;

public:
  MmapWritableBuffer(const std::string& path, size_t size);
  ~MmapWritableBuffer();

private:
  void realloc(size_t newsize) override;
  void map(int fd, size_t size);
  void unmap();
};


#endif
