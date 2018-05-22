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

class MemoryRangeImpl;
class IntegrityCheckContext;


//==============================================================================
// MemoryRange
//==============================================================================

/**
 * MemoryRange class represents a contiguous chunk of memory. This memory
 * chunk may be shared across multiple MemoryRange instances: this allows
 * MemoryRange objects to be copied easily.
 *
 * Internally, MemoryRange is implemented by one of the MemoryRangeImpl
 * backends. There are implementations for:
 *   - plain memory storage (MemoryMRI);
 *   - memory owned by an external source (ExternalMRI);
 *   - view onto another MemoryRange (ViewMRI).
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
    MemoryRangeImpl* impl;

  public:
    // Basic copy & move constructors.
    // The copy constructor creates a shallow copy (similar to shared_ptr).
    // The move constructor leaves the source object in a state where the
    // only legal operation is to destruct that object.
    MemoryRange();
    MemoryRange(const MemoryRange&);
    MemoryRange(MemoryRange&&);
    MemoryRange& operator=(const MemoryRange&);
    MemoryRange& operator=(MemoryRange&&);
    ~MemoryRange();

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
    //   been allocated using `std::malloc`.
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

    size_t size() const;
    bool is_writeable() const;
    bool is_resizable() const;
    bool is_pyobjects() const;
    operator bool() const;

    const void* rptr() const;
    const void* rptr(size_t offset) const;
    void* wptr();
    void* wptr(size_t offset);
    template <typename T> T get_element(int64_t i) const;
    template <typename T> void set_element(int64_t i, T value);

    MemoryRange& set_pyobjects(bool clear_data);

    MemoryRange& resize(size_t newsize, bool keep_data = true);

    // Write the content of the memory range to `file`. If the file already
    // exists, it will be overwritten.
    void save_to_disk(
      const std::string& file,
      WritableBuffer::Strategy strategy = WritableBuffer::Strategy::Auto
    );

    PyObject* pyrepr() const;
    size_t memory_footprint() const;
    bool verify_integrity(IntegrityCheckContext& icc) const;

  private:
    void materialize(size_t newsize, size_t copysize);
};


template <> void MemoryRange::set_element(int64_t, PyObject*);
extern template int32_t MemoryRange::get_element(int64_t) const;
extern template int64_t MemoryRange::get_element(int64_t) const;
extern template void MemoryRange::set_element(int64_t, PyObject*);
extern template void MemoryRange::set_element(int64_t, int32_t);
extern template void MemoryRange::set_element(int64_t, int64_t);


#endif
