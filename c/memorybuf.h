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


#endif
