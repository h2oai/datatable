//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_MEMRANGE_h
#define dt_MEMRANGE_h
#include <string>             // std::string
#include <type_traits>        // std::is_same
#include <Python.h>
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "writebuf.h"

class ViewedMRI;
class BaseMRI;
class IntegrityCheckContext;


//==============================================================================
// MemoryRange
//==============================================================================

/**
 * MemoryRange class represents a contiguous chunk of memory. This memory
 * chunk may be shared across multiple MemoryRange instances: this allows
 * MemoryRange objects to be copied with negligible overhead.
 *
 * Internally, MemoryRange object contains a single member: pointer to a
 * `MemoryRangeObject` structure. This structure in turn contains a pointer
 * to a `BaseMRI` object and a refcounter. The `BaseMRI` object is instantiated
 * from one of the derived classes (representing different backends):
 *   - plain memory storage (MemoryMRI);
 *   - memory owned by an external source (ExternalMRI);
 *   - view onto another MemoryRange (ViewMRI);
 *   - MemoryRange that is currently being "viewed" (ViewedMRI);
 *   - memory-mapped file (MmapMRI).
 * This 2-tiered structure allows us to replace an internal `BaseMRI` object
 * with another implementation, if needed -- without having to modify any of
 * the user-facing `MemoryRange` objects.
 *
 * The class implements Copy-on-Write semantics: if a user wants to write into
 * the memory buffer contained in a MemoryRange object, and that memory buffer
 * is currently shared with other MemoryRange instances, then the class will
 * first replace the current memory buffer with a new copy belonging only to
 * the current class.
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
    struct internal;
    std::shared_ptr<internal> o;

  public:
    // Basic copy & move constructors / assignment operators.
    // The copy constructor creates a shallow copy (similar to shared_ptr).
    // The move constructor leaves the source object in a state where the
    // only legal operation is to destruct that object.
    //
    MemoryRange();
    MemoryRange(const MemoryRange&) = default;
    MemoryRange(MemoryRange&&) = default;
    MemoryRange& operator=(const MemoryRange&) = default;
    MemoryRange& operator=(MemoryRange&&) = default;

    // Factory constructors:
    //
    // MemoryRange(n)
    //   Allocate memory region of size `n` in memory (on the heap). The memory
    //   will be freed when the MemoryRange object goes out of scope (assuming
    //   no shallow copies were created).
    //
    // MemoryRange(n, ptr, own)
    //   Create MemoryRange from an existing pointer `ptr` to a memory buffer
    //   of size `n`. If `own` is false, the ownership of the pointer will
    //   not be assumed: the caller will be responsible for deallocating `ptr`
    //   when it is no longer needed, but not before the MemoryRange object is
    //   deleted. However, if `own` is true, then the MemoryRange object will
    //   take ownership of that pointer. In this case the `ptr` should have had
    //   been allocated using `dt::malloc`.
    //
    // MemoryRange(n, ptr, pybuf)
    //   Create MemoryRange from a pointer `ptr` to a memory buffer of size `n`
    //   and using `pybuf` as the guard for the memory buffer's lifetime. The
    //   `pybuf` here is a `Py_buffer` struct used to implement Python buffers
    //   interface. The MemoryRange object created in this way is neither
    //   writeable nor resizeable.
    //
    // MemoryRange(n, src, offset)
    //   Create MemoryRange as a "view" onto another MemoryRange `src`. The
    //   view is positioned at `offset` from the beginning of `src`s buffer,
    //   and has the length `n`.
    //
    MemoryRange(size_t n);
    MemoryRange(size_t n, void* ptr, bool own);
    MemoryRange(size_t n, const void* ptr, Py_buffer* pybuf);
    MemoryRange(size_t n, MemoryRange& src, size_t offset);
    MemoryRange(const std::string& path);
    MemoryRange(size_t n, const std::string& path, int fd = -1);
    MemoryRange(const std::string& path, size_t nextra, int fd = -1);

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
    template <typename T> T get_element(int64_t i) const;
    template <typename T> void set_element(int64_t i, T value);

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
    // save_to_disk(file, strategy)
    //   Write the content of the memory range to `file`. If the file already
    //   exists, it will be overwritten.
    //
    //
    MemoryRange& set_pyobjects(bool clear_data);
    MemoryRange& resize(size_t newsize, bool keep_data = true);
    void save_to_disk(
      const std::string& file,
      WritableBuffer::Strategy strategy = WritableBuffer::Strategy::Auto
    );

    // Utility functions
    //
    // pyrepr()
    //   Return PyObject* containing `repr()` string of this object. [Not sure
    //   if this function ought to exist].
    //
    // verify_integrity(icc)
    //   Check internal validity of this object.
    //
    PyObject* pyrepr() const;
    bool verify_integrity(IntegrityCheckContext& icc) const;

  private:
    // Helper function for dealing with non-writeable objects. It will replace
    // the current `impl` with a new `MemoryMRI` object of size `newsize`,
    // containing copy of first `copysize` bytes of the `impl`'s data. It is
    // assumed that `copysize ≤ newsize`.
    // The no-arguments form does pure copy (i.e. newsize = copysize = bufsize)
    void materialize(size_t newsize, size_t copysize);
    void materialize();

    friend BaseMRI;
    friend ViewedMRI;
};


template <> void MemoryRange::set_element(int64_t, PyObject*);
extern template int32_t MemoryRange::get_element(int64_t) const;
extern template int64_t MemoryRange::get_element(int64_t) const;
extern template void MemoryRange::set_element(int64_t, PyObject*);
extern template void MemoryRange::set_element(int64_t, char);
extern template void MemoryRange::set_element(int64_t, int32_t);
extern template void MemoryRange::set_element(int64_t, int64_t);


#endif
