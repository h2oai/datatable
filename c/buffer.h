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
namespace py { class buffer; }


//==============================================================================
// Buffer
//==============================================================================

/**
  * Buffer class represents a contiguous chunk of memory. This memory
  * chunk may be shared across multiple Buffer instances: this allows
  * Buffer objects to be copied with negligible overhead.
  *
  * The class implements Copy-on-Write semantics: if a user wants to
  * write into the memory buffer contained in a Buffer object, and
  * that memory buffer is currently shared with other Buffer instances,
  * then the class will first replace its internal impl with a
  * writable copy of the memory buffer.
  *
  * The class may also be marked as "containing PyObjects". In this
  * case the contents of the buffer will receive special treatment:
  *   - The length of the data buffer must be a multiple of
  *     sizeof(PyObject*).
  *   - Each element in the data buffer must be a valid PyObject*
  *     pointer at all times: this is why `set_contains_pyobjects()`
  *     has a boolean flag whether the data needs to be initialized
  *     to contain `Py_None`s, or whether the buffer already contains
  *      valid `PyObject*`s.
  *   - When such array is deallocated, all its elements are DECREFed.
  *   - When the array is copied for the purpose of CoW semantics, the
  *     elements in the copied array are INCREFed.
  *   - When the array is resized upwards, the newly added elements
  *     are initialized to `Py_None`s.
  *   - When the array is resized downwards, the elements that
  *     disappear will be DECREFed.
  *   - get_element() returns a borrowed reference to the requested
  *     element.
  *   - set_element() takes a new reference and stores it into the
  *     array, DECREFing the element being overwritten.
  *
  */
class Buffer
{
  private:
    BufferImpl* impl_;  // shared-pointer semantics

  public:
    // Basic copy & move constructors / assignment operators.
    // The copy constructor creates a shallow copy (similar to shared_ptr).
    // The move constructor leaves the source object in a state where the
    // only legal operation is to destruct that object.
    //
    Buffer();
    Buffer(const Buffer&);
    Buffer(Buffer&&);
    Buffer& operator=(const Buffer&);
    Buffer& operator=(Buffer&&);
    ~Buffer();

    // Factory constructors:
    //
    // Buffer::mem(n)
    //   Allocate memory region of size `n` in memory (on the heap). The memory
    //   will be freed when the Buffer object goes out of scope (assuming
    //   no shallow copies were created).
    //
    // Buffer::copy(ptr, n)
    //   Allocate a memory region of size `n` in memory, and copy the contents
    //   of pointer `ptr` there.
    //
    // Buffer::acquire(ptr, n)
    //   Create Buffer from an existing pointer `ptr` to a memory buffer
    //   of size `n`. The ownership of `ptr` will be transferred to the
    //   Buffer object. In this case the `ptr` should have had been
    //   allocated using `dt::malloc`.
    //
    // Buffer::external(ptr, n)
    //   Create Buffer from an existing pointer `ptr` to a memory buffer
    //   of size `n`, however the ownership of the pointer will not be assumed:
    //   the caller will be responsible for deallocating `ptr` when it is no
    //   longer in use, but not before the Buffer object is deleted.
    //
    // Buffer::external(ptr, n, pybuf)
    //   Create Buffer from a pointer `ptr` to a memory buffer of size `n`
    //   and using `pybuf` as the guard for the memory buffer's lifetime. The
    //   `pybuf` here is a `Py_buffer` struct used to implement Python buffers
    //   interface. The Buffer object created in this way is neither
    //   writeable nor resizeable.
    //
    // Buffer::view(src, n, offset)
    //   Create Buffer as a "view" onto another Buffer `src`. The
    //   view is positioned at `offset` from the beginning of `src`s buffer,
    //   and has the length `n`.
    //
    // Buffer:mmap(path)
    //   Create Buffer by mem-mapping a file given by the `path`.
    //
    // Buffer::mmap(path, n, [fd])
    //   Create a file of size `n` at `path`, and then memory-map it.
    //
    // Buffer::overmap(path, nextra)
    //   Similar to `mmap(path)`, but the memmap will return a buffer
    //   over-allocated for `nextra` bytes above the size of the file. This is
    //   used mostly in fread.
    //
    static Buffer mem(size_t n);
    static Buffer mem(int64_t n);
    static Buffer copy(const void* ptr, size_t n);
    static Buffer acquire(void* ptr, size_t n);
    static Buffer external(void* ptr, size_t n);
    static Buffer external(const void* ptr, size_t n);
    static Buffer external(const void* ptr, size_t n, py::buffer&& pybuf);
    static Buffer view(const Buffer& src, size_t n, size_t offset);
    static Buffer mmap(const std::string& path);
    static Buffer mmap(const std::string& path, size_t n, int fd = -1);
    static Buffer overmap(const std::string& path, size_t nextra, int fd = -1);

    // Basic properties of the Buffer:
    //
    // size()
    //   Size of the memory in bytes.
    //
    // operator bool()
    //   Return true if the memory allocation is non-empty, i.e. size() > 0.
    //
    // is_writable()
    //   Return true if modifying data in this Buffer is allowed. This may
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
    //   Return total size in bytes taken by this Buffer object. This
    //   includes the size of the memory buffer itself, plus the sizes of all
    //   auxiliary variables.
    //
    size_t size() const;
    operator bool() const;
    bool is_writable() const;
    bool is_resizable() const;
    bool is_pyobjects() const;
    size_t memory_footprint() const noexcept;

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
    //   If the Buffer is marked as "pyobjects" then the getter will return
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

    // Buffer manipulators
    //
    // set_pyobjects(clear_data)
    //   Marks the Buffer as "pyobjects" (there is no function to remove
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
    // to_memory()
    //   Force a memory-mapped buffer into the main memory; noop for other
    //   buffer types.
    //
    Buffer& set_pyobjects(bool clear_data);
    Buffer& resize(size_t newsize, bool keep_data = true);
    void to_memory();

    // Utility functions
    //
    // verify_integrity()
    //   Check internal validity of this object.
    //
    void verify_integrity() const;

  private:
    explicit Buffer(BufferImpl*&& impl);

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

#if DTDEBUG
  inline void buffer_oob_check(size_t i, size_t size, size_t elemsize) {
    if ((i + 1) * elemsize > size) {
      throw ValueError() << "Index " << i << " is out of bounds for a buffer "
        "of size " << size << " bytes when each element's size is " << elemsize;
    }
  }
#else
  inline void buffer_oob_check(size_t, size_t, size_t) {}
#endif

template <typename T>
T Buffer::get_element(size_t i) const {
  buffer_oob_check(i, size(), sizeof(T));
  const T* data = static_cast<const T*>(this->rptr());
  return data[i];
}

template <>
inline void Buffer::set_element(size_t i, PyObject* value) {
  buffer_oob_check(i, size(), sizeof(PyObject*));
  xassert(this->is_pyobjects());
  auto data = static_cast<PyObject**>(this->wptr());
  Py_SETREF(data[i], value);
}

template <typename T>
void Buffer::set_element(size_t i, T value) {
  buffer_oob_check(i, size(), sizeof(T));
  T* data = static_cast<T*>(this->wptr());
  data[i] = value;
}



#endif
