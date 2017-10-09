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
#ifndef dt_MEMORYBUF_H
#define dt_MEMORYBUF_H
#include <Python.h>
#include <stdbool.h>
#include <string>


//==============================================================================

/**
 * Abstract interface that encapsulates data which can be stored in different
 * "backends".
 */
class MemoryBuffer
{
  void*  buf;
  size_t allocsize;
  bool   readonly;
  int64_t : 56;  // padding


  //--- Public API -------------------------------------------------------------
public:
  // This class is not directly constructible: construct one of the derived
  // classes instead.
  MemoryBuffer(const MemoryBuffer&) = delete;  // copy-constructor
  MemoryBuffer(MemoryBuffer&&) = delete;       // move-constructor
  virtual ~MemoryBuffer();

  /**
   * Returns a void* pointer to the underlying memory region (`get()`) or to the
   * specific offset within that region (`at()`). The offset is assumed to be
   * given in bytes. Thus, `get()` is equivalent to `at(0)`. The returned
   * pointer can be nullptr if the memory was not allocated.
   *
   * The multiple `at()` methods all do the same, they exist only to spare the
   * user from having to cast their integer offset into a proper integer type.
   */
  void* get() const;
  void* at(size_t offset) const;
  void* at(int64_t offset) const;
  void* at(int32_t offset) const;

  /**
   * Treats the memory buffer as an array `T[]` and retrieves / sets its `i`-th
   * element. Note that type `T` is not attached to the `MemoryBuffer` class
   * itself. This allows us to easily re-interpret the buffer as having a
   * different underlying type, if needed.
   *
   * These functions do not perform any array bound checks. It is thus the
   * responsibility of the caller to ensure that `i * sizeof(T) < size()`.
   * Failure to do so will lead to memory corruption / seg.fault.
   */
  template <typename T> T get_elem(int64_t i) const;
  template <typename T> void set_elem(int64_t i, T value);

  /**
   * Returns the allocation size of the underlying memory buffer. This will be
   * zero if memory is unallocated.
   */
  size_t size() const;

  /**
   * Returns the best estimate of this object's total size in memory. This is
   * comprised of the allocated size for the underlying memory buffer, as well
   * as the size of the object itself, and sizes of all its member objects.
   *
   * The value returned by this method is platform-dependent, and may even
   * change between subsequent runs of the program. Derived classes should
   * override this function if they have any internal members.
   */
  virtual size_t memory_footprint() const;

  /**
   * Returns true if the memory buffer is marked read-only. A read-only buffer
   * cannot be resized, nor its contents changed. These restrictions however
   * are not enforced by the class itself -- instead it is the responsibility
   * of the caller. Attempting to modify a readonly buffer may cause exceptions
   * or seg.faults.
   */
  bool is_readonly() const;

  /**
   * Changes allocation size of the memory region to be exactly `n` bytes. If
   * current allocation size is less than `n`, the buffer will be expanded
   * retaining the existing data. If current allocation size is greater than
   * `n`, the buffer will be shrunk, truncating data at the end.
   *
   * This function is similar to `realloc(buf, n)` in standard C. An exception
   * will be thrown if the buffer could not be resized. This function must be
   * overridden in the derived classes.
   */
  virtual void resize(size_t n);

  /**
   * Returns short python string (PyUnicodeObject*) describing the class of
   * this MemoryBuffer object. The returned python object is a "New reference",
   * and the caller is expected to DECREF it once it no longer needs this
   * object.
   *
   * This method must be overridden in all subclasses. It is the equivalent of
   * old "MType" enum, and is used for debugging purposes from the Python side.
   * This method may be removed in the future.
   */
  virtual PyObject* pyrepr() const;


  //--- Internal ---------------------------------------------------------------
protected:
  MemoryBuffer();
  void replace_buffer(void* ptr, size_t sz);
  void set_readonly(bool on = true);
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
  virtual void resize(size_t n) override;
  virtual PyObject* pyrepr() const override;
  virtual ~MemoryMemBuf() override;
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
  virtual PyObject* pyrepr() const override;
};



//==============================================================================

/**
 * MemoryBuffer corresponding to an external memory region guarded by a
 * `Py_buffer` object. This memory was retrieved from an external application
 * (such as numpy or pandas), and should not be modified or freed. Instead, when
 * the column is deallocated we "release" the buffer, allowing its owner to
 * dispose of that buffer if it is no longer in use.
 */
class ExternalMemBuf : public MemoryBuffer
{
  void* pybufinfo;

public:
  ExternalMemBuf(void *ptr, void *pybuf, size_t size);
  virtual size_t memory_footprint() const override;
  virtual PyObject* pyrepr() const override;
  virtual ~ExternalMemBuf() override;
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
   * Create a memory region backed by a memory-mapped disk file.
   *
   * This constructor may either map an existing file (when `create = false`),
   * or create a new one (if `create = true`). Specifically, when `create` is
   * true, then `path` must be a valid path in the file system (it may or may
   * not point to an existing file), and `n` should be the desired file size
   * in bytes. Conversely, when `create` is false, then `path` must correspond
   * to an existing accessible file, and parameter `n` is ignored.
   */
  MemmapMemBuf(const char *path, size_t n, bool create);

  virtual ~MemmapMemBuf() override;
  virtual void resize(size_t n) override;
  virtual size_t memory_footprint() const override;
  virtual PyObject* pyrepr() const override;
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

template <typename T> T MemoryBuffer::get_elem(int64_t i) const {
  return (static_cast<T*>(buf))[i];
}

template <typename T> void MemoryBuffer::set_elem(int64_t i, T value) {
  (static_cast<T*>(buf))[i] = value;
}

#endif
