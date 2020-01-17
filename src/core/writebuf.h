//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_WRITEBUF_H
#define dt_WRITEBUF_H
#include <memory>      // std::unique_ptr
#include <string>      // std::string
#include "parallel/shared_mutex.h"
#include "utils/file.h"
using std::size_t;

class Buffer;



//==============================================================================

class WritableBuffer
{
protected:
  size_t bytes_written;

public:
  enum class Strategy : int8_t { Unknown, Auto, Mmap, Write };

  WritableBuffer();
  virtual ~WritableBuffer();

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
   *
   * @param append
   *     If true, the file will be opened in append mode ("a"); if false, the
   *     file will be opened in overwrite mode ("w").
   */
  static std::unique_ptr<WritableBuffer> create_target(
    const std::string& path, size_t size, Strategy strategy = Strategy::Auto,
    bool append = false
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
  virtual size_t prep_write(size_t n, const void* src) = 0;

  /**
   * Write buffer `src` of size `n` at the position `pos` (this position should
   * have previously been returned from `pre_write()`, which also ensured that
   * there is enough space to perform the writing).
   *
   * This call is safe to invoke from multiple threads simultaneously. It is
   * also allowed to call this method when another thread is performing
   * `prep_write()`.
   */
  virtual void write_at(size_t pos, size_t n, const void* src) = 0;

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
  void write(size_t n, const void* src) {
    write_at(prep_write(n, src), n, src);
  }
  void write(const CString& src) {
    size_t at = prep_write(static_cast<size_t>(src.size), src.ch);
    write_at(at, static_cast<size_t>(src.size), src.ch);
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
  FileWritableBuffer(const std::string& path, bool append = false);
  virtual ~FileWritableBuffer() override;

  virtual size_t prep_write(size_t n, const void* src) override;
  virtual void write_at(size_t pos, size_t n, const void* src) override;
  virtual void finalize() override;
};



//==============================================================================

class ThreadsafeWritableBuffer : public WritableBuffer
{
protected:
  void*  buffer;
  size_t allocsize;
  dt::shared_mutex shmutex;

  virtual void realloc(size_t newsize) = 0;

public:
  ThreadsafeWritableBuffer();
  virtual ~ThreadsafeWritableBuffer() override;

  virtual size_t prep_write(size_t n, const void* src) override;
  virtual void write_at(size_t pos, size_t n, const void* src) override;
  virtual void finalize() override;
};



//==============================================================================

class MemoryWritableBuffer : public ThreadsafeWritableBuffer
{
public:
  MemoryWritableBuffer(size_t size);
  ~MemoryWritableBuffer() override;

  /**
   * Return memory buffer that was written. This method may only be called
   * after `finalize()`. This class surrenders ownership of the buffer, and
   * it will be the responsibility of the caller to handle it.
   */
  Buffer get_mbuf();
  std::string get_string();

private:
  void realloc(size_t newsize) override;
};



//==============================================================================

class MmapWritableBuffer : public ThreadsafeWritableBuffer
{
  std::string filename;

public:
  MmapWritableBuffer(const std::string& path, size_t size, bool append = false);
  ~MmapWritableBuffer() override;

  void finalize() override;

private:
  void realloc(size_t newsize) override;
  void map(int fd, size_t size);
  void unmap();
};


#endif
