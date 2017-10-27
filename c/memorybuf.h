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
#include "datatable_check.h"


class MemoryMemBuf;


//==============================================================================

/**
 * Abstract interface that encapsulates data which can be stored in different
 * "backends". This class is a wrapper around a `void*` pointer to some memory
 * region.
 */
class MemoryBuffer
{
protected:
  // Reference count for this MemoryBuffer, in case it is shared by multiple
  // users.
  int refcount;

  // Readonly flag: indicates that the memory region cannot be written into,
  // and cannot be resized.
  bool readonly;

  int32_t : 24;  // padding


  //--- Public API -------------------------------------------------------------
public:
  // This class is not directly constructible: construct one of the derived
  // classes instead. Alternatively, if you need to shallow-copy the current
  // object, use `membuf->shallowcopy()`.
  MemoryBuffer(const MemoryBuffer&) = delete;  // copy-constructor
  MemoryBuffer(MemoryBuffer&&) = delete;       // move-constructor

  /**
   * Returns a void* pointer to the underlying memory region (`get()`) or to the
   * specific offset within that region (`at()`). The offset is assumed to be
   * given in bytes. Thus, `get()` is equivalent to `at(0)`. The returned
   * pointer can be nullptr if the memory was not allocated.
   *
   * The multiple `at()` methods all do the same, they exist only to spare the
   * user from having to cast their integer offset into a proper integer type.
   */
  virtual void* get() const = 0;
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
   * Returns the allocation size of the underlying memory buffer. This should be
   * zero if memory is unallocated.
   */
  virtual size_t size() const = 0;

  /**
   * Returns the best estimate of this object's total size in memory. This is
   * comprised of the allocated size for the underlying memory buffer, as well
   * as the size of the object itself, and sizes of all its member objects.
   *
   * The value returned by this method is platform-dependent, and may even
   * change between subsequent runs of the program.
   */
  virtual size_t memory_footprint() const = 0;

  /**
   * Returns true if the memory buffer is marked read-only. A read-only buffer
   * cannot be resized, nor its contents changed. These restrictions however
   * are not enforced by the class itself -- instead it is the responsibility
   * of the caller. Attempting to modify a readonly buffer may cause exceptions
   * or seg.faults.
   *
   * This method also returns true if the internal reference counter is > 1,
   * indicating that the memory buffer is shared among multiple users. Modifying
   * such shared memory is not allowed, because one of the users will not be
   * aware of the changes, and may crash as a result.
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
   * Similar to `resize(n)`, but can be applied to readonly memory buffers too.
   * In that case this method will create a new `MemoryMemBuf` object, resize
   * it, and copy the contents of the current buffer. In both cases the resized
   * buffer will be returned. Also note that if the returned buffer is different
   * from `this`, then `this` will be "released", i.e. it is assumed that the
   * caller uses this method as follows:
   *
   *   membuf = membuf->safe_resize(n);
   *
   * This method also guarantees that the returned buffer is not readonly. Thus,
   * it the current buffer is readonly, a new buffer will be created even when
   * `n == size()`.
   */
  MemoryBuffer* safe_resize(size_t n);

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

  /**
   * Returns a new reference to the current object. For all intents and purposes
   * this works as a copy of an object, except that no actual copy is made.
   */
  MemoryBuffer* shallowcopy();

  /**
   * Create and return a new MemoryMemBuf which is a "deep" copy of this
   * MemoryBuffer. Note that a "deep" copy is always an instance of MemoryMemBuf
   * class, regardless of the class of the current object.
   */
  MemoryMemBuf* deepcopy() const;

  /**
   * This method should be called where you would normally say `delete membuf;`.
   * Since this MemoryBuffer may be shared across multiple objects, this method
   * decrements the internal reference counter, and only when the counter
   * reaches zero deletes the object.
   */
  void release();

  /**
   * Returns the internal ref counter of the MemoryBuffer. This is useful mostly
   * for debugging purposes.
   */
  int get_refcount() const;


  virtual bool verify_integrity(IntegrityCheckContext&,
                                const std::string& name = "MemoryBuffer") const;


  //--- Internal ---------------------------------------------------------------
protected:
  MemoryBuffer();
  virtual ~MemoryBuffer();  // Private, use membuf->release() instead
};




//==============================================================================

/**
 * Memory-based MemoryBuffer. Using this class is equivalent to standard C
 * functions malloc/realloc/free.
 */
class MemoryMemBuf : public MemoryBuffer
{
  void* buf;
  size_t allocsize;

public:
  /**
   * Allocate `n` bytes of memory and wrap this pointer into a new MemoryMemBuf
   * object. An exception is raised if the memory cannot be allocated. The case
   * `n = 0` is also valid: it will create an "empty" MemoryMemBuf without
   * allocating any memory.
   */
  MemoryMemBuf(size_t n);

  /**
   * Create MemoryMemBuf of size `n` and baised on the pointer `ptr`. An
   * exception will be raised if `n` is positive and `ptr` is null. This
   * constructor assumes ownership of pointer `ptr` and will free it when
   * MemoryMemBuf is deleted. If you don't want MemoryMemBuf to assume
   * ownership of the pointer, then use :class:`ExternalMemBuf` instead.
   */
  MemoryMemBuf(void* ptr, size_t n);

  void* get() const override;
  size_t size() const override;
  size_t memory_footprint() const override;
  PyObject* pyrepr() const override;
  virtual void resize(size_t n) override;
  bool verify_integrity(IntegrityCheckContext&,
                        const std::string& n = "MemoryBuffer") const override;

private:
  MemoryMemBuf();
  virtual ~MemoryMemBuf();
};



//==============================================================================

/**
 * MemoryBuffer corresponding to a readonly external memory region. "External"
 * in this case means that the object will not manage the memory region, and
 * rely on the calling code to eventually free all resources (but not before
 * this object is deleted!).
 */
class ExternalMemBuf : public MemoryBuffer
{
  void* buf;
  size_t allocsize;
  void* pybufinfo;

public:
  /**
   * Create an ExternalMemBuf of size `n` based on the provided pointer `ptr`
   * and guarded by the provided `Py_buffer` object. When this object is
   * deleted, it will call `PyBuffer_Release()` handler signaling the owner
   * that the pointer `ptr` is no longer in use.
   */
  ExternalMemBuf(void* ptr, void* pybuf, size_t n);

  /**
   * Create an ExternalMemBuf of size `n` based on the provided pointer `ptr`.
   * The object will not assume the ownership of the pointer, and will not
   * attempt to free it in the end. It is the responsibility of the user to
   * ensure that the pointer is not freed prematurely.
   */
  ExternalMemBuf(void* ptr, size_t n);

  /**
   * Create an ExternalMemBuf mapped onto a C string. The C string is not
   * considered "owned" by this class, and its memory will not be deallocated
   * when the object is destroyed. It is the responsibility of the user of
   * this class to ensure that the lifetime of the source string exceeds the
   * lifetime of the constructed ExternalMemBuf object.
   */
  ExternalMemBuf(const char* cstr);

  void* get() const override;
  size_t size() const override;
  size_t memory_footprint() const override;
  PyObject* pyrepr() const override;
  bool verify_integrity(IntegrityCheckContext&,
                        const std::string& n = "MemoryBuffer") const override;

private:
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
  void* buf;
  size_t allocsize;
  const std::string filename;

public:
  /**
   * Open and memory-map an existing file (first constructor), or create a new
   * file of size `n` and then memory-map it (second constructor).
   */
  MemmapMemBuf(const std::string& file);
  MemmapMemBuf(const std::string& file, size_t n);

  void* get() const override;
  size_t size() const override;
  virtual void resize(size_t n) override;
  virtual size_t memory_footprint() const override;
  virtual PyObject* pyrepr() const override;
  virtual bool verify_integrity(
    IntegrityCheckContext&, const std::string& n = "MemoryBuffer"
  ) const override;

protected:
  /**
   * This constructor may either map an existing file (when `create = false`),
   * or create a new one (if `create = true`). Specifically, when `create` is
   * true, then `path` must be a valid path in the file system (it may or may
   * not point to an existing file), and `n` should be the desired file size
   * in bytes. Conversely, when `create` is false, then `path` must correspond
   * to an existing accessible file, and parameter `n` is ignored.
   */
  MemmapMemBuf(const std::string& path, size_t n, bool create);
  virtual ~MemmapMemBuf();
};



//==============================================================================

/**
 * A variant of MemmapMemBuf that attempts to overallocate the memory region
 * by a specific number of bytes. This is used in fread.
 */
class OvermapMemBuf : public MemmapMemBuf
{
  void* xbuf;
  size_t xbuf_size;

public:
  OvermapMemBuf(const std::string& path, size_t n);

  virtual void resize(size_t n) override;
  virtual size_t memory_footprint() const override;
  virtual PyObject* pyrepr() const override;
  virtual bool verify_integrity(
    IntegrityCheckContext&, const std::string& n = "MemoryBuffer"
  ) const override;

protected:
  virtual ~OvermapMemBuf();
};



//==============================================================================
// Template implementations

template <typename T> T MemoryBuffer::get_elem(int64_t i) const {
  return (static_cast<T*>(get()))[i];
}

template <typename T> void MemoryBuffer::set_elem(int64_t i, T value) {
  (static_cast<T*>(get()))[i] = value;
}

#endif
