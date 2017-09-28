#ifndef dt_MEMORYBUF_H
#define dt_MEMORYBUF_H
#include <stdbool.h>

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

  MemoryBuffer(): buf(NULL), allocsize(0), flags(0) {}
  bool owned() const {
    return (flags & MB_EXTERNAL) == 0;
  }

public:
  /**
   * Return pointer to the underlying memory region. The returned pointer can
   * be NULL if the memory was not allocated.
   */
  void* get() const {
    return buf;
  }

  /**
   * Return the allocation size of this memory buffer.
   */
  size_t size() const {
    return allocsize;
  }

  /**
   * Return true if the memory buffer is marked as readonly. Trying to modify
   * the contents of such buffer may lead to undesired consequences.
   */
  bool readonly() const {
    return (flags & MB_READONLY) != 0;
  }

  /**
   * Change allocation size of the memory region to be exactly `n` bytes. If
   * current allocation size is less than `n`, the buffer will be expanded
   * retaining the existing data. If current allocation size if greater than
   * `n`, the buffer will be shrunk, truncating data at the end.
   *
   * This function is equivalent to `realloc(buf, n)`.
   */
  virtual void resize(size_t n) = 0;

  /**
   * Ensure that at least `n` bytes in the buffer is available. If not, then
   * the buffer will be resized to hold at least `n * factor` bytes (ensuring
   * that resizes do not happen too often). Passing values of `factor` less
   * than 1 will break this function's contract.
   */
  void ensuresize(size_t n, double factor=1.3) {
    if (n <= allocsize) return;
    resize(static_cast<size_t>(factor * n));
  }

  virtual ~MemoryBuffer();
};



//==============================================================================

/**
 * Memory-based MemoryBuffer. Using this class is equivalent to standard C
 * functions malloc/realloc/free.
 */
class RamMemoryBuffer : public MemoryBuffer
{
public:
  RamMemoryBuffer(size_t n);
  virtual void resize(size_t n);
  virtual ~RamMemoryBuffer();
};



//==============================================================================

/**
 * MemoryBuffer which is mapped onto a C string. This buffer is immutable.
 * The string is not considered "owned" by this class, and its memory will not
 * be deallocated when the object is destroyed.
 */
class StringMemoryBuffer : public MemoryBuffer
{
public:
  StringMemoryBuffer(const char *str);
  virtual void resize(size_t);
  virtual ~StringMemoryBuffer();
};



//==============================================================================

/**
 * MemoryBuffer backed by a disk file and mem-mapped into the process' memory.
 * This class supports both memmapping existing files, and creating new files
 * and then mapping them. This also includes creating temporary files.
 *
 * This class can also be used to save an existing memory buffer onto disk:
 * just create a new MmapMemoryBuffer object, and then memcpy the data into
 * its memory region.
 */
class MmapMemoryBuffer : public MemoryBuffer
{
  const char *filename;

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
  MmapMemoryBuffer(const char *path, size_t n=0, int flags=0);

  virtual void resize(size_t n);

  virtual ~MmapMemoryBuffer();
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
};



//==============================================================================

class FileWritableBuffer : WritableBuffer
{
  int fd;

public:
  FileWritableBuffer(const char *path);
  virtual ~FileWritableBuffer();

  virtual size_t prep_write(size_t n, const void *src);
  virtual void write_at(size_t pos, size_t n, const void *src);
  virtual void finalize();

  // Prevent copying / assignment for these objects
  FileWritableBuffer(const FileWritableBuffer&) = delete;
  FileWritableBuffer& operator=(const FileWritableBuffer&) = delete;
};



//==============================================================================

class MemoryWritableBuffer : WritableBuffer
{
  void*  buffer;
  size_t allocsize;
  int    nlocks;

public:
  MemoryWritableBuffer(size_t size);
  virtual ~MemoryWritableBuffer();

  virtual size_t prep_write(size_t n, const void *src);
  virtual void write_at(size_t pos, size_t n, const void *src);
  virtual void finalize();

  /**
   * Return memory buffer that was written. This method may only be called
   * after `finalize()`. This class surrenders ownership of the buffer, and
   * it will be the responsibility of the caller to handle it.
   */
  void* get();

  // Prevent copying / assignment for these objects
  MemoryWritableBuffer(const MemoryWritableBuffer&) = delete;
  MemoryWritableBuffer& operator=(const MemoryWritableBuffer&) = delete;
};


#endif
