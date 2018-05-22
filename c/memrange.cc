//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "memrange.h"
#include <cstdlib>             // std::malloc, std::free
#include "datatable_check.h"   // IntegrityCheckContext
#include "memorybuf.h"         // ?
#include "utils/exceptions.h"  // ValueError, MemoryError



//==============================================================================
// Implementation classes
//==============================================================================

  class MemoryRangeImpl {
    public:
      void*  bufdata;
      size_t bufsize;
      int    refcount;
      bool   pyobjects;
      bool   writeable;
      bool   resizable;
      int: 8;

    public:
      MemoryRangeImpl();
      inline void release();

      virtual void resize(size_t n) = 0;
      virtual size_t memory_footprint() const = 0;
      virtual const char* name() const = 0;
      virtual ~MemoryRangeImpl();
  };


  class MemoryMRI : public MemoryRangeImpl {
    public:
      MemoryMRI(size_t n);
      MemoryMRI(size_t n, void* ptr);

      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "ram"; }
      ~MemoryMRI() override;
  };


  class ExternalMRI : public MemoryRangeImpl {
    private:
      Py_buffer* pybufinfo;

    public:
      ExternalMRI(size_t n, const void* ptr);
      ExternalMRI(size_t n, const void* ptr, Py_buffer* pybuf);
      ExternalMRI(const char* str);

      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "ext"; }
      ~ExternalMRI() override;
  };


  class ViewMRI : public MemoryRangeImpl {
    private:
      const MemoryRange& source;
      size_t offset;

    public:
      ViewMRI(size_t n, MemoryRange& src, size_t offset);

      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "view"; }
  };


  class MmapMRI : public MemoryRangeImpl {
    private:
      const std::string filename;
      size_t mmm_index;
      int fd;
      bool mapped;
      int : 24;

    public:
      MmapMRI(const std::string& path);
      MmapMRI(size_t n, const std::string& path, int fd);
      MmapMRI(size_t n, const std::string& path, int fd, bool create);

      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "mmap"; }
  };




//==============================================================================
// Main `MemoryRange` class
//==============================================================================

  //---- Basic constructors ------------

  MemoryRange::MemoryRange() {
    impl = new MemoryMRI(0);
  }

  MemoryRange::MemoryRange(const MemoryRange& other) {
    impl = other.impl;
    impl->refcount++;
  }

  MemoryRange::MemoryRange(MemoryRange&& other) {
    impl = other.impl;
    other.impl = nullptr;
  }

  MemoryRange& MemoryRange::operator=(const MemoryRange& other) {
    impl->release();
    impl = other.impl;
    impl->refcount++;
    return *this;
  }

  MemoryRange& MemoryRange::operator=(MemoryRange&& other) {
    impl->release();
    impl = other.impl;
    other.impl = nullptr;
    return *this;
  }

  MemoryRange::~MemoryRange() {
    // impl can be NULL if its content was moved out via `std::move()`
    if (impl) impl->release();
  }


  //---- Factory constructors ----------

  MemoryRange::MemoryRange(size_t n) {
    impl = new MemoryMRI(n);
  }

  MemoryRange::MemoryRange(size_t n, void* ptr, bool own) {
    if (own) impl = new MemoryMRI(n, ptr);
    else     impl = new ExternalMRI(n, ptr);
  }

  MemoryRange::MemoryRange(size_t n, const void* ptr, Py_buffer* pb) {
    impl = new ExternalMRI(n, ptr, pb);
  }

  MemoryRange::MemoryRange(size_t n, MemoryRange& src, size_t offset) {
    impl = new ViewMRI(n, src, offset);
  }

  MemoryRange::MemoryRange(const std::string& path) {
    impl = new MmapMRI(path);
  }

  MemoryRange::MemoryRange(size_t n, const std::string& path, int fd) {
    impl = new MmapMRI(n, path, fd);
  }



  MemoryRange& MemoryRange::set_pyobjects(bool clear_data) {
    size_t n = impl->bufsize / sizeof(PyObject*);
    xassert(n * sizeof(PyObject*) == impl->bufsize);
    xassert(this->is_writable());
    if (clear_data) {
      PyObject** data = static_cast<PyObject**>(impl->bufdata);
      for (size_t i = 0; i < n; ++i) {
        data[i] = Py_None;
      }
      Py_None->ob_refcnt += n;
    }
    impl->pyobjects = true;
    return *this;
  }


  MemoryRange::operator bool() const {
    return (impl->bufsize != 0);
  }

  bool MemoryRange::is_writable() const {
    return (impl->refcount == 1) && impl->writeable;
  }

  bool MemoryRange::is_pyobjects() const {
    return impl->pyobjects;
  }

  size_t MemoryRange::size() const {
    return impl->bufsize;
  }



  const void* MemoryRange::rptr() const {
    return impl->bufdata;
  }

  const void* MemoryRange::rptr(size_t offset) const {
    return static_cast<const void*>(
              static_cast<const char*>(impl->bufdata) + offset);
  }

  void* MemoryRange::wptr() {
    if (!is_writable()) {
      MemoryMRI* newimpl = new MemoryMRI(impl->bufsize);
      std::memcpy(newimpl->bufdata, impl->bufdata, impl->bufsize);
      impl->release();
      impl = newimpl;
    }
    return impl->bufdata;
  }

  void* MemoryRange::wptr(size_t offset) {
    return static_cast<void*>(static_cast<char*>(wptr()) + offset);
  }


  MemoryRange& MemoryRange::resize(size_t newsize, bool keep_data) {
    bool pyobj_mode = impl->pyobjects;
    if (newsize == impl->bufsize) {}
    else if (newsize) {
      size_t currsize = impl->bufsize;
      if (is_writable()) {
        if (pyobj_mode && newsize < currsize) {
          // TODO
        }
        impl->resize(newsize);
      } else {
        MemoryMRI* newimpl = new MemoryMRI(newsize);
        if (keep_data) {
          size_t copysize = std::min(newsize, currsize);
          std::memcpy(newimpl->bufdata, impl->bufdata, copysize);
          if (pyobj_mode) {
            newimpl->pyobjects = true;
            PyObject** newdata = static_cast<PyObject**>(newimpl->bufdata);
            size_t n = copysize / sizeof(PyObject*);
            for (size_t i = 0; i < n; ++i) {
              Py_INCREF(newdata[i]);
            }
          }
        }
        impl->release();
        impl = newimpl;
      }
    } else {
      impl->release();
      impl = new MemoryMRI(0);
      impl->pyobjects = pyobj_mode;
    }
    return *this;
  }



  void MemoryRange::save_to_disk(const std::string& path,
                                  WritableBuffer::Strategy strategy)
  {
    auto wb = WritableBuffer::create_target(path, impl->bufsize, strategy);
    wb->write(impl->bufsize, impl->bufdata);
  }


  PyObject* MemoryRange::pyrepr() const {
    return PyUnicode_FromFormat("<MemoryRange:%s %p+%zu (ref=%zu)>",
                                impl->name(), impl->bufdata, impl->bufsize,
                                impl->refcount);
  }

  size_t MemoryRange::memory_footprint() const {
    return sizeof(MemoryRange) + impl->memory_footprint();
  }

  bool MemoryRange::verify_integrity(IntegrityCheckContext& icc) const {
    return true;
  }


  //---- Element getters/setters -------

  static void _oob_check(int64_t i, size_t size, size_t elemsize) {
    if (i < 0 || static_cast<size_t>(i + 1) * elemsize > size) {
      throw ValueError() << "Index " << i << " is out of bounds for a memory "
        "region of size " << size << " viewed as an array of elements of size "
        << elemsize;
    }
  }

  template <typename T>
  T MemoryRange::get_element(int64_t i) const {
    _oob_check(i, size(), sizeof(T));
    const T* data = static_cast<const T*>(this->rptr());
    return data[i];
  }

  template <>
  void MemoryRange::set_element(int64_t i, PyObject* value) {
    _oob_check(i, size(), sizeof(PyObject*));
    xassert(this->is_pyobjects());
    PyObject** data = static_cast<PyObject**>(this->wptr());
    Py_DECREF(data[i]);
    data[i] = value;
  }

  template <typename T>
  void MemoryRange::set_element(int64_t i, T value) {
    _oob_check(i, size(), sizeof(T));
    T* data = static_cast<T*>(this->wptr());
    data[i] = value;
  }



//==============================================================================
// MemoryRangeImpl
//==============================================================================

  MemoryRangeImpl::MemoryRangeImpl()
    : bufdata(nullptr),
      bufsize(0),
      refcount(1),
      pyobjects(false),
      writeable(true),
      resizable(true) {}

  MemoryRangeImpl::~MemoryRangeImpl() {}


  void MemoryRangeImpl::release() {
    if (--refcount == 0) {
      // If memory buffer contains `PyObject*`s, then they must be DECREFed
      // before being deleted.
      if (bufdata && pyobjects) {
        PyObject** items = static_cast<PyObject**>(bufdata);
        size_t n = bufsize / sizeof(PyObject*);
        for (size_t i = 0; i < n; ++i) {
          Py_DECREF(items[i]);
        }
      }
      delete this;
    }
  }




//==============================================================================
// MemoryMRI
//==============================================================================

  MemoryMRI::MemoryMRI(size_t n) {
    if (n) {
      bufsize = n;
      bufdata = std::malloc(n);
      if (bufdata == nullptr) {
        throw MemoryError() << "Unable to allocate memory of size " << n;
      }
    }
  }

  MemoryMRI::MemoryMRI(size_t n, void* ptr) {
    if (n) {
      if (!ptr) throw ValueError() << "Unallocated memory region provided";
      bufsize = n;
      bufdata = ptr;
    }
  }

  MemoryMRI::~MemoryMRI() {
    std::free(bufdata);
  }


  void MemoryMRI::resize(size_t n) {
    // The documentation for `void* realloc(void* ptr, size_t new_size);` says
    // the following:
    // | If there is not enough memory, the old memory block is not freed and
    // | null pointer is returned.
    // | If new_size is zero, the behavior is implementation defined (null
    // | pointer may be returned (in which case the old memory block may or may
    // | not be freed), or some non-null pointer may be returned that may not be
    // | used to access storage). Support for zero size is deprecated as of
    // | C11 DR 400.
    if (n == bufsize) return;
    if (n) {
      void* ptr = std::realloc(bufdata, n);
      if (!ptr) {
        throw MemoryError() << "Unable to reallocate memory to size " << n;
      }
      bufdata = ptr;
    } else if (bufdata) {
      std::free(bufdata);
      bufdata = nullptr;
    }
    bufsize = n;
  }


  size_t MemoryMRI::memory_footprint() const {
    return sizeof(MemoryMRI) + bufsize;
  }




//==============================================================================
// ExternalMRI
//==============================================================================

  ExternalMRI::ExternalMRI(size_t size, const void* ptr, Py_buffer* pybuf) {
    if (!ptr && size > 0) {
      throw ValueError() << "Unallocated buffer supplied to the ExternalMRI "
                         << "constructor. Expected memory region of size "
                         << size;
    }
    bufdata = const_cast<void*>(ptr);
    bufsize = size;
    pybufinfo = pybuf;
  }

  ExternalMRI::ExternalMRI(size_t n, const void* ptr)
      : ExternalMRI(n, ptr, nullptr) {}

  ExternalMRI::ExternalMRI(const char* str)
      : ExternalMRI(strlen(str) + 1, str, nullptr) {}

  ExternalMRI::~ExternalMRI() {
    if (pybufinfo) {
      PyBuffer_Release(pybufinfo);
    }
  }

  void ExternalMRI::resize(size_t) {
    throw Error() << "Unable to resize ExternalMRI buffer";
  }

  size_t ExternalMRI::memory_footprint() const {
    return sizeof(ExternalMRI) + bufsize + (pybufinfo? sizeof(Py_buffer) : 0);
  }


  // bool ExternalMRI::verify_integrity(IntegrityCheckContext& icc,
  //                                       const std::string& name) const
  // {
  //   int nerrs = icc.n_errors();
  //   MemoryBuffer::verify_integrity(icc, name);
  //   // Not much we can do about checking the validity of `buf`, unfortunately. It
  //   // is provided by an external source, and could in theory point to anything...
  //   if (bufsize && !buf) {
  //     icc << "Internal data pointer in " << name << " is null" << icc.end();
  //   }
  //   return !icc.has_errors(nerrs);
  // }




//==============================================================================
// ViewMRI
//==============================================================================

  ViewMRI::ViewMRI(size_t n, MemoryRange& src, size_t offs)
    : source(src), offset(offs)
  {
    xassert(src.is_writable());
    xassert(offs + n <= src.size());
    bufdata = src.wptr(offs);
    bufsize = n;
  }

  void ViewMRI::resize(size_t) {
    throw Error() << "Cannot resize a View MemoryRange";
  }

  size_t ViewMRI::memory_footprint() const {
    return sizeof(ViewMRI) + bufsize;
  }




//==============================================================================
// MmapMRI
//==============================================================================

  MmapMRI::MmapMRI(const std::string& path)
    : MmapMRI(0, path, -1, false) {}

  MmapMRI::MmapMRI(size_t n, const std::string& path, int fileno)
    : MmapMRI(n, path, fileno, true) {}

  MmapMRI::MmapMRI(size_t n, const std::string& path, int fileno, bool create) {

  }

  void MmapMRI::resize(size_t n) {

  }

  size_t MmapMRI::memory_footprint() const {
    return sizeof(MmapMRI) + filename.size() + (mapped? bufsize : 0);
  }




//==============================================================================
// Template instantiations
//==============================================================================

  template int32_t MemoryRange::get_element(int64_t) const;
  template int64_t MemoryRange::get_element(int64_t) const;
  template void MemoryRange::set_element(int64_t, int32_t);
  template void MemoryRange::set_element(int64_t, int64_t);
  template void MemoryRange::set_element(int64_t, PyObject*);
