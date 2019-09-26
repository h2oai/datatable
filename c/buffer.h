//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#ifndef dt_BUFFER_h
#define dt_BUFFER_h
#include <cstdint>
#include <memory>             // std::unique_ptr
#include <string>             // std::string
#include <type_traits>        // std::is_same
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "writebuf.h"

class BufferImpl;


//==============================================================================
// MemoryRange
//==============================================================================

/**
 * MemoryRange class represents a contiguous chunk of memory. This memory
 * chunk may be shared across multiple MemoryRange instances: this allows
 * MemoryRange objects to be copied with negligible overhead.
 *
 * Internally, MemoryRange object contains just a single `shared_ptr<internal>`
 * object `o`. This shared pointer allows `MemoryRange` to be easily copyable.
 * The "internal" struct contains a `unique_ptr<BufferImpl> impl` pointer, whereas
 * the BufferImpl object can actually be instantiated into any of the derived
 * classes (representing different backends):
 *   - plain memory storage (MemoryMRI);
 *   - memory owned by an external source (ExternalMRI);
 *   - view onto another MemoryRange (ViewMRI);
 *   - MemoryRange that is currently being "viewed" (ViewedMRI);
 *   - memory-mapped file (MmapMRI).
 * This 2-tiered structure allows us to replace the internal `BufferImpl` object
 * with another implementation, if needed -- without having to modify any of
 * the user-facing `MemoryRange` objects.
 *
 * The class implements Copy-on-Write semantics: if a user wants to write into
 * the memory buffer contained in a MemoryRange object, and that memory buffer
 * is currently shared with other MemoryRange instances, then the class will
 * first replace its internal impl with a writable copy of the memory buffer.
 *
 * The class may also be marked as "containing PyObjects". In this case the
 * contents of the buffer will receive special treatment:
 *   - The length of the data buffer must be a multiple of sizeof(PyObject*).
 *   - Each element in the data buffer must be a valid PyObject* pointer at
 *     all times: this is why `set_contains_pyobjects()` has a boolean flag
 *     whether the data needs to be initialized to contain `Py_None`s, or
 *     whether the buffer already contains valid `PyObject*`s.
 *   - When such array is deallocated, all its elements are DECREFed.
 *   - When the array is copied for the purpose of CoW semantics, the elements
 *     in the copied array are INCREFed.
 *   - When the array is resized upwards, the newly added elements are
 *     initialized to `Py_None`s.
 *   - When the array is resized downwards, the elements that disappear
 *     will be DECREFed.
 *   - get_element() returns a borrowed reference to the requested element.
 *   - set_element() takes a new reference and stores it into the array,
 *     DECREFing the element being overwritten.
 *
 */
class MemoryRange
{
  private:
    BufferImpl* impl_;  // shared-pointer semantics

  public:
    // Basic copy & move constructors / assignment operators.
    // The copy constructor creates a shallow copy (similar to shared_ptr).
    // The move constructor leaves the source object in a state where the
    // only legal operation is to destruct that object.
    //
    MemoryRange();
    MemoryRange(const MemoryRange&);
    MemoryRange(MemoryRange&&);
    MemoryRange& operator=(const MemoryRange&);
    MemoryRange& operator=(MemoryRange&&);
    ~MemoryRange();

    // Factory constructors:
    //
    // MemoryRange::mem(n)
    //   Allocate memory region of size `n` in memory (on the heap). The memory
    //   will be freed when the MemoryRange object goes out of scope (assuming
    //   no shallow copies were created).
    //
    // MemoryRange::acquire(ptr, n)
    //   Create MemoryRange from an existing pointer `ptr` to a memory buffer
    //   of size `n`. The ownership of `ptr` will be transferred to the
    //   MemoryRange object. In this case the `ptr` should have had been
    //   allocated using `dt::malloc`.
    //
    // MemoryRange::external(ptr, n)
    //   Create MemoryRange from an existing pointer `ptr` to a memory buffer
    //   of size `n`, however the ownership of the pointer will not be assumed:
    //   the caller will be responsible for deallocating `ptr` when it is no
    //   longer in use, but not before the MemoryRange object is deleted.
    //
    // MemoryRange::external(ptr, n, pybuf)
    //   Create MemoryRange from a pointer `ptr` to a memory buffer of size `n`
    //   and using `pybuf` as the guard for the memory buffer's lifetime. The
    //   `pybuf` here is a `Py_buffer` struct used to implement Python buffers
    //   interface. The MemoryRange object created in this way is neither
    //   writeable nor resizeable.
    //
    // MemoryRange::view(src, n, offset)
    //   Create MemoryRange as a "view" onto another MemoryRange `src`. The
    //   view is positioned at `offset` from the beginning of `src`s buffer,
    //   and has the length `n`.
    //
    // MemoryRange:mmap(path)
    //   Create MemoryRange by mem-mapping a file given by the `path`.
    //
    // MemoryRange::mmap(path, n, [fd])
    //   Create a file of size `n` at `path`, and then memory-map it.
    //
    // MemoryRange::overmap(path, nextra)
    //   Similar to `mmap(path)`, but the memmap will return a buffer
    //   over-allocated for `nextra` bytes above the size of the file. This is
    //   used mostly in fread.
    //
    static MemoryRange mem(size_t n);
    static MemoryRange mem(int64_t n);
    static MemoryRange acquire(void* ptr, size_t n);
    static MemoryRange external(const void* ptr, size_t n);
    static MemoryRange external(const void* ptr, size_t n, Py_buffer* pybuf);
    static MemoryRange view(const MemoryRange& src, size_t n, size_t offset);
    static MemoryRange mmap(const std::string& path);
    static MemoryRange mmap(const std::string& path, size_t n, int fd = -1);
    static MemoryRange overmap(const std::string& path, size_t nextra,
                               int fd = -1);

    // Basic properties of the MemoryRange:
    //
    // size()
    //   Size of the memory in bytes.
    //
    // operator bool()
    //   Return true if the memory allocation is non-empty, i.e. size() > 0.
    //
    // is_writable()
    //   Return true if modifying data in this MemoryRange is allowed. This may
    //   return false in one of the two cases: (1) either the data is inherently
    //   read-only (e.g. opened from a file, or from external memory region
    //   passed via pybuffers interface); or (2) reference count on the data is
    //   greater than 1.
    //
    // is_resizable()
    //   Is it possible to resize the data? If the data is resizable, then it
    //   is also writeable, but not the other way around.
    //
    // is_pyobjects()
    //   The data is marked as an array of `PyObject*`s. This case receives
    //   special treatment during memory allocation / deallocation / resizing /
    //   copying in order to maintain correct reference counters on those
    //   `PyObject*`s. The data is not allowed to be uninitialized; it will be
    //   filled with `Py_None`s during resizing.
    //
    // memory_footprint()
    //   Return total size in bytes taken by this MemoryRange object. This
    //   includes the size of the memory buffer itself, plus the sizes of all
    //   auxiliary variables.
    //
    size_t size() const;
    operator bool() const;
    bool is_writable() const;
    bool is_resizable() const;
    bool is_pyobjects() const;
    size_t memory_footprint() const;

    // Main data accessors
    //
    // rptr()
    // rptr(offset)
    //   Return the internal data pointer as `const void*`, i.e. as read-only
    //   memory. This always returns `impl->bufdata`, unmodified.
    //
    // wptr()
    // wptr(offset)
    //   Return the internal data pointer as `void*`, i.e. suitable for writing.
    //   This will return `impl->bufdata` if and only if `is_writable()` is
    //   true, otherwise `impl` will be replaced with a new copy of the data
    //   and only then the new data pointer will be returned. This implements
    //   the Copy-on-Write semantics.
    //
    // xptr()
    // xptr(offset)
    //   Similar to `wptr()`: return a `void*` pointer suitable for writing. The
    //   difference is that if the underlying implementation is not writable, an
    //   exception will be raised instead of creating a Copy-on-Write.
    //
    // get_element<T>(i)
    // set_element<T>(i, value)
    //   Getter/setter for individual entries in the memory buffer when it is
    //   viewed as an array `T[]`. These methods perform bounds checks on `i`,
    //   and therefore should not be used in performance-critical code.
    //   If the MemoryRange is marked as "pyobjects" then the getter will return
    //   a "borrowed reference" object, while the setter will "steal" the
    //   ownership of `value`.
    //
    const void* rptr() const;
    const void* rptr(size_t offset) const;
    void* wptr();
    void* wptr(size_t offset);
    void* xptr() const;
    void* xptr(size_t offset) const;
    template <typename T> T get_element(size_t i) const;
    template <typename T> void set_element(size_t i, T value);

    // MemoryRange manipulators
    //
    // set_pyobjects(clear_data)
    //   Marks the MemoryRange as "pyobjects" (there is no function to remove
    //   such a mark). The flag `clear_data` controls what to do with the
    //   existing data. If the flag is true, then current data is cleared and
    //   the buffer is filled with `Py_None` objects. In this case the data
    //   buffer must be writeable. If the flag is `false`, then the user says
    //   that the data buffer already contains valid `PyObject*` objects that
    //   should not be cleared. The function returns `this` for convenience.
    //
    // resize(newsize, keep_data)
    //   Change the size of the buffer. This will be done in-place if
    //   `is_resizable()` is true, otherwise `impl` will be replaced with a
    //   new memory buffer that is both writeable and resizable. The boolean
    //   flag `keep_data` is a hint from the user whether he/she cares about
    //   the existing data in the buffer or not. If this flag is false
    //   (default is true) then the implementation *may* replace the current
    //   data with garbage bytes or it may leave them intact.
    //
    MemoryRange& set_pyobjects(bool clear_data);
    MemoryRange& resize(size_t newsize, bool keep_data = true);

    // Utility functions
    //
    // verify_integrity()
    //   Check internal validity of this object.
    //
    void verify_integrity() const;

  private:
    explicit MemoryRange(BufferImpl*&& impl);

    // Helper function for dealing with non-writeable objects. It will replace
    // the current `impl` with a new `MemoryMRI` object of size `newsize`,
    // containing copy of first `copysize` bytes of the `impl`'s data. It is
    // assumed that `copysize ≤ newsize`.
    // The no-arguments form does pure copy (i.e. newsize = copysize = bufsize)
    void materialize(size_t newsize, size_t copysize);
    void materialize();
};



//------------------------------------------------------------------------------
// Template definitions
//------------------------------------------------------------------------------

inline void buffer_oob_check(size_t i, size_t size, size_t elemsize) {
  if ((i + 1) * elemsize > size) {
    throw ValueError() << "Index " << i << " is out of bounds for a buffer "
      "of size " << size << " bytes when each element's size is " << elemsize;
  }
}

template <typename T>
T MemoryRange::get_element(size_t i) const {
  buffer_oob_check(i, size(), sizeof(T));
  const T* data = static_cast<const T*>(this->rptr());
  return data[i];
}

template <>
inline void MemoryRange::set_element(size_t i, PyObject* value) {
  buffer_oob_check(i, size(), sizeof(PyObject*));
  xassert(this->is_pyobjects());
  PyObject** data = static_cast<PyObject**>(this->wptr());
  Py_DECREF(data[i]);
  data[i] = value;
}

template <typename T>
void MemoryRange::set_element(size_t i, T value) {
  buffer_oob_check(i, size(), sizeof(T));
  T* data = static_cast<T*>(this->wptr());
  data[i] = value;
}



#endif
