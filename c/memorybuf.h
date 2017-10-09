#ifndef dt_MEMORYBUF_H
#define dt_MEMORYBUF_H
#include <string>
#include <stdbool.h>
#include "Python.h"

#define MB_EXTERNAL  1
#define MB_READONLY  2
#define MB_CREATE    4



//==============================================================================

/**
 * Abstract interface that encapsulates data which can be stored in different
 * "backends".
 */
class MemoryBuffer
{
protected:
  void *buf;
  size_t allocsize;
  int flags;
  int _padding;

  MemoryBuffer();
  bool owned() const;

public:
  /**
   * Return pointer to the underlying memory region. The returned pointer can
   * be NULL if the memory was not allocated.
   */
  void* get() const;

  /**
   * Return pointer to the specific offset within the buffer. The offset is
   * assumed to be given in bytes. `at(0)` is equivalent to `get()`.
   */
  void* at(size_t n) const;
  void* at(int64_t n) const;
  void* at(int32_t n) const;

  template <typename T>
  void set_elem(size_t i, T value);

  /**
   * Return the allocation size of this memory buffer.
   */
  size_t size() const;

  /**
   * Return the total size of this object in memory. This consists not only of
   * the underlying memory buffer, but also all other "auxiliary" items in the
   * class.
   */
  virtual size_t memory_footprint() const;

  /**
   * Return true if the memory buffer is marked as readonly. Trying to modify
   * the contents of such buffer may lead to undesired consequences.
   */
  bool readonly() const;

  /**
   * Change allocation size of the memory region to be exactly `n` bytes. If
   * current allocation size is less than `n`, the buffer will be expanded
   * retaining the existing data. If current allocation size if greater than
   * `n`, the buffer will be shrunk, truncating data at the end.
   *
   * This function is equivalent to `realloc(buf, n)`.
   */
  virtual void resize(size_t n);

  /**
   * Ensure that at least `n` bytes in the buffer is available. If not, then
   * the buffer will be resized to hold at least `n * factor` bytes (ensuring
   * that resizes do not happen too often). Passing values of `factor` less
   * than 1 will break this function's contract.
   */
  void ensuresize(size_t n, double factor=1.3);

  virtual PyObject* pyrepr() const;

  virtual ~MemoryBuffer();

  MemoryBuffer(const MemoryBuffer&) = delete;  // copy-constructor
  MemoryBuffer(MemoryBuffer&&);  // move-constructor
  MemoryBuffer& operator=(MemoryBuffer&&); // move-assignment
};




//==============================================================================

/**
 * Memory-based MemoryBuffer. Using this class is equivalent to standard C
 * functions malloc/realloc/free.
 */
class MemoryMemBuf : public MemoryBuffer
{
public:
  MemoryMemBuf(size_t n);
  MemoryMemBuf(void *ptr, size_t n);
  virtual void resize(size_t n);
  virtual PyObject* pyrepr() const;
  virtual ~MemoryMemBuf();
};



//==============================================================================

/**
 * MemoryBuffer which is mapped onto a C string. This buffer is immutable.
 * The string is not considered "owned" by this class, and its memory will not
 * be deallocated when the object is destroyed.
 */
class StringMemBuf : public MemoryBuffer
{
public:
  StringMemBuf(const char *str);
  virtual void resize(size_t);
  virtual PyObject* pyrepr() const;
  virtual ~StringMemBuf();
};



//==============================================================================

/**
 */
class ExternalMemBuf : public MemoryBuffer
{
  void* pybufinfo;

public:
  ExternalMemBuf(void *ptr, void *pybuf, size_t size);
  virtual void resize(size_t);
  virtual size_t memory_footprint() const;
  virtual PyObject* pyrepr() const;
  virtual ~ExternalMemBuf();
};



//==============================================================================

/**
 * MemoryBuffer backed by a disk file and mem-mapped into the process' memory.
 * This class supports both memmapping existing files, and creating new files
 * and then mapping them. This also includes creating temporary files.
 *
 * This class can also be used to save an existing memory buffer onto disk:
 * just create a new MemmapMemBuf object, and then memcpy the data into
 * its memory region.
 */
class MemmapMemBuf : public MemoryBuffer
{
  std::string filename;

public:
  /**
   * Create a memory region backed by a disk file.
   * This constructor may either map an existing file, or create a new one.
   *
   * More specifically, if MB_CREATE flag is provided, then
   *   - `path` must not correspond to any existing file,
   *   - `n` should be the desired size of the file, in bytes,
   *   - `flags` should be MB_CREATE, potentially combined with MB_EXTERNAL.
   *     When MB_EXTERNAL is set, the file is considered "permanent", otherwise
   *     the file is assumed to be temporary, and will be removed when this
   *     memory buffer is deallocated.
   *
   * When MB_CREATE is not given, then
   *   - `path` must point to an existing accessible file,
   *   - `n` is ignored,
   *   - `flags` should be MB_EXTERNAL, potentially combined with MB_READONLY.
   *     When MB_READONLY is set, the file will be opened in readonly mode;
   *     otherwise it will be opened in read-write mode.
   */
  MemmapMemBuf(const char *path, size_t n=0, int flags=0);

  virtual void resize(size_t n);
  virtual size_t memory_footprint() const;
  virtual PyObject* pyrepr() const;

  virtual ~MemmapMemBuf();
};



//==============================================================================

class WritableBuffer
{
protected:
  size_t bytes_written;

public:
  WritableBuffer(): bytes_written(0) {}
  virtual ~WritableBuffer() {}

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
protected:
  int fd;
  int _unused;

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
  int    nlocks;
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
protected:
  virtual void realloc(size_t newsize);

public:
  MemoryWritableBuffer(size_t size);
  virtual ~MemoryWritableBuffer();

  /**
   * Return memory buffer that was written. This method may only be called
   * after `finalize()`. This class surrenders ownership of the buffer, and
   * it will be the responsibility of the caller to handle it.
   */
  void* get();
};



//==============================================================================

class MmapWritableBuffer : public ThreadsafeWritableBuffer
{
protected:
  std::string filename;

  virtual void realloc(size_t newsize);

public:
  MmapWritableBuffer(const std::string& path, size_t size);
  virtual ~MmapWritableBuffer();
};



//==============================================================================
// Template implementations

template <typename T>
void MemoryBuffer::set_elem(size_t i, T value) {
  (static_cast<T*>(buf))[i] = value;
}

#endif
