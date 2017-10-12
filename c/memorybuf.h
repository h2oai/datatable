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
 * "backends". This class is a wrapper around a `void*` pointer to some memory
 * region.
 */
class MemoryBuffer
{
protected:
  // Pointer to the "underlying" memory region. This class knows how to read/
  // write to this memory region, but not how to manage it (i.e. allocate /
  // resize / free) -- this is the responsibility of each derived class. This is
  // also the reason why the fields in this class are protected instead of
  // private: the base class cannot handle these fields by itself.
  void* buf;

  // Size of the memory region `buf`, in bytes. (In practice, the memory region
  // may have arbitrary "size" -- this field merely says how many bytes should
  // be accessible to the user).
  size_t allocsize;

  // Reference count for this MemoryBuffer, in case it is shared by multiple
  // users.
  int refcount;

  // Readonly flag: indicates that the memory region cannot be written into,
  // and cannot be resized.
  bool readonly;

  int64_t : 24;  // padding


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
public:
  MemoryMemBuf(size_t n);
  MemoryMemBuf(void *ptr, size_t n);
  explicit MemoryMemBuf(const MemoryBuffer&);
  virtual void resize(size_t n) override;
  virtual PyObject* pyrepr() const override;

private:
  virtual ~MemoryMemBuf();
};



//==============================================================================

/**
 * MemoryBuffer which is mapped onto a C string. This buffer is immutable.
 * The string is not considered "owned" by this class, and its memory will not
 * be deallocated when the object is destroyed. It is the responsibility of the
 * user of this class to ensure that the lifetime of the source string exceeds
 * the lifetime of a StringMemBuf object.
 */
class StringMemBuf : public MemoryBuffer
{
public:
  StringMemBuf(const char *str);
  virtual PyObject* pyrepr() const override;

private:
  virtual ~StringMemBuf();
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

  virtual void resize(size_t n) override;
  virtual size_t memory_footprint() const override;
  virtual PyObject* pyrepr() const override;

private:
  virtual ~MemmapMemBuf();
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
