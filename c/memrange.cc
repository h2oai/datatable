//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "memrange.h"
#include <cerrno>              // errno
#include <cstdlib>             // std::malloc, std::free
#include <mutex>               // std::mutex, std::lock_guard
#include <sys/mman.h>          // mmap, munmap
#include "datatable_check.h"   // IntegrityCheckContext
#include "memorybuf.h"         // ?
#include "mmm.h"               // MemoryMapWorker, MemoryMapManager
#include "utils/exceptions.h"  // ValueError, MemoryError



//==============================================================================
// Implementation classes
//==============================================================================

  class MemoryRangeImpl {
    protected:
      void*  bufdata;
      size_t bufsize;

    public:
      int    refcount;
      bool   pyobjects;
      bool   writeable;
      bool   resizable;
      int: 8;

    public:
      MemoryRangeImpl();
      virtual void release();

      virtual void resize(size_t) {}
      virtual size_t size() const { return bufsize; }
      virtual void* ptr() const { return bufdata; }
      virtual size_t memory_footprint() const = 0;
      virtual const char* name() const = 0;
      virtual bool verify_integrity(IntegrityCheckContext& icc) const;
      virtual ~MemoryRangeImpl();
  };


  class MemoryMRI : public MemoryRangeImpl {
    public:
      MemoryMRI(size_t n);
      MemoryMRI(size_t n, void* ptr);

      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "ram"; }
      bool verify_integrity(IntegrityCheckContext& icc) const override;
      ~MemoryMRI() override;
  };


  class ExternalMRI : public MemoryRangeImpl {
    private:
      Py_buffer* pybufinfo;

    public:
      ExternalMRI(size_t n, void* ptr);
      ExternalMRI(size_t n, const void* ptr, Py_buffer* pybuf);
      ExternalMRI(const char* str);

      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "ext"; }
      ~ExternalMRI() override;
  };


  // ViewMRI represents a memory range which is a part of a larger memory
  // region controlled by MemoryRange `source`.
  //
  // TODO: needs more work!
  //
  class ViewMRI : public MemoryRangeImpl {
    private:
      const MemoryRange& source;
      size_t offset;

    public:
      ViewMRI(size_t n, MemoryRange& src, size_t offset);

      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "view"; }
      bool verify_integrity(IntegrityCheckContext& icc) const override;
  };
  /*
  class ViewedMRI : public MemoryRangeImpl {
    private:
      MemoryRange* source_memoryrange;
      MemoryRangeImpl* original_impl;
      int view_refcount;
      int : 32;

    public:
      ViewedMRI(MemoryRange* src, MemoryRangeImpl* impl);
      void release() override;
      void release_view();
      size_t memory_footprint() const override { return 0; }
      const char* name() const override { return "viewed"; }
      ~ViewedMRI() override;
  };
  */


  class MmapMRI : public MemoryRangeImpl, MemoryMapWorker {
    private:
      const std::string filename;
      size_t mmm_index;
      int fd;
      bool mapped;
      bool temporary_file;
      int : 16;

    public:
      MmapMRI(const std::string& path);
      MmapMRI(size_t n, const std::string& path, int fd);
      MmapMRI(size_t n, const std::string& path, int fd, bool create);
      ~MmapMRI();

      void* ptr() const override;
      size_t size() const override;
      void save_entry_index(size_t i) override;
      void evict() override;
      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "mmap"; }

    private:
      void memmap();
      void memunmap();
  };




//==============================================================================
// Main `MemoryRange` class
//==============================================================================

  //---- Basic constructors ----------------------

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


  //---- Factory constructors --------------------

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


  //---- Basic properties ------------------------

  MemoryRange::operator bool() const {
    return (impl->size() != 0);
  }

  bool MemoryRange::is_writeable() const {
    return (impl->refcount == 1) && impl->writeable;
  }

  bool MemoryRange::is_resizable() const {
    return (impl->refcount == 1) && impl->resizable;
  }

  bool MemoryRange::is_pyobjects() const {
    return impl->pyobjects;
  }

  size_t MemoryRange::size() const {
    return impl->size();
  }

  size_t MemoryRange::memory_footprint() const {
    return sizeof(MemoryRange) + impl->memory_footprint();
  }


  //---- Main data accessors ---------------------

  const void* MemoryRange::rptr() const {
    return impl->ptr();
  }

  const void* MemoryRange::rptr(size_t offset) const {
    const char* ptr0 = static_cast<const char*>(rptr());
    return static_cast<const void*>(ptr0 + offset);
  }

  void* MemoryRange::wptr() {
    if (!is_writeable()) materialize();
    return impl->ptr();
  }

  void* MemoryRange::wptr(size_t offset) {
    char* ptr0 = static_cast<char*>(wptr());
    return static_cast<void*>(ptr0 + offset);
  }


  //---- MemoryRange manipulators ----------------

  MemoryRange& MemoryRange::set_pyobjects(bool clear_data) {
    size_t n = impl->size() / sizeof(PyObject*);
    xassert(n * sizeof(PyObject*) == impl->size());
    xassert(this->is_writeable());
    if (clear_data) {
      PyObject** data = static_cast<PyObject**>(impl->ptr());
      for (size_t i = 0; i < n; ++i) {
        data[i] = Py_None;
      }
      Py_None->ob_refcnt += n;
    }
    impl->pyobjects = true;
    return *this;
  }

  MemoryRange& MemoryRange::resize(size_t newsize, bool keep_data) {
    size_t oldsize = impl->size();
    if (newsize != oldsize) {
      if (is_resizable()) {
        if (impl->pyobjects) {
          size_t n_old = oldsize / sizeof(PyObject*);
          size_t n_new = newsize / sizeof(PyObject*);
          if (n_new < n_old) {
            PyObject** data = static_cast<PyObject**>(impl->ptr());
            for (size_t i = n_new; i < n_old; ++i) Py_DECREF(data[i]);
          }
          impl->resize(newsize);
          if (n_new > n_old) {
            PyObject** data = static_cast<PyObject**>(impl->ptr());
            for (size_t i = n_old; i < n_new; ++i) data[i] = Py_None;
            Py_None->ob_refcnt += n_new - n_old;
          }
        } else {
          impl->resize(newsize);
        }
      } else {
        size_t copysize = keep_data? std::min(newsize, oldsize) : 0;
        materialize(newsize, copysize);
      }
    }
    return *this;
  }


  //---- Utility functions -----------------------

  void MemoryRange::save_to_disk(const std::string& path,
                                  WritableBuffer::Strategy strategy)
  {
    auto wb = WritableBuffer::create_target(path, impl->size(), strategy);
    wb->write(impl->size(), impl->ptr());
  }


  PyObject* MemoryRange::pyrepr() const {
    return PyUnicode_FromFormat("<MemoryRange:%s %p+%zu (ref=%zu)>",
                                impl->name(), impl->ptr(), impl->size(),
                                impl->refcount);
  }

  bool MemoryRange::verify_integrity(IntegrityCheckContext& icc) const {
    if (!impl) {
      icc << "NULL implementation object in MemoryRange" << icc.end();
      return false;
    }
    return impl->verify_integrity(icc);
  }

  void MemoryRange::materialize() {
    size_t s = impl->size();
    materialize(s, s);
  }

  void MemoryRange::materialize(size_t newsize, size_t copysize) {
    xassert(newsize >= copysize);
    MemoryMRI* newimpl = new MemoryMRI(newsize);
    if (copysize) {
      std::memcpy(newimpl->ptr(), impl->ptr(), copysize);
    }
    if (impl->pyobjects) {
      newimpl->pyobjects = true;
      PyObject** newdata = static_cast<PyObject**>(newimpl->ptr());
      size_t n_new = newsize / sizeof(PyObject*);
      size_t n_copy = copysize / sizeof(PyObject*);
      size_t i = 0;
      for (; i < n_copy; ++i) Py_INCREF(newdata[i]);
      for (; i < n_new; ++i) newdata[i] = Py_None;
      Py_None->ob_refcnt += n_new - n_copy;
    }
    impl->release();
    impl = newimpl;
  }


  //---- Element getters/setters -----------------

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

  MemoryRangeImpl::~MemoryRangeImpl() {
    // If memory buffer contains `PyObject*`s, then they must be DECREFed
    // before being deleted.
    if (bufdata && pyobjects) {
      PyObject** items = static_cast<PyObject**>(bufdata);
      size_t n = bufsize / sizeof(PyObject*);
      for (size_t i = 0; i < n; ++i) {
        Py_DECREF(items[i]);
      }
    }
  }

  void MemoryRangeImpl::release() {
    if (--refcount == 0) delete this;
  }

  bool MemoryRangeImpl::verify_integrity(IntegrityCheckContext& icc) const {
    if (refcount <= 0) {
      icc << "Invalid refcount in MemoryRange: " << refcount << icc.end();
      return false;
    }
    if (!bufdata && bufsize) {
      icc << "MemoryRange has bufdata = NULL but size = " << bufsize
          << icc.end();
      return false;
    }
    if (bufdata && !bufsize) {
      icc << "MemoryRange has bufdata = " << bufdata << " but size = 0"
          << icc.end();
      return false;
    }
    if (resizable && !writeable) {
      icc << "MemoryRange is resizable but not writeable" << icc.end();
      return false;
    }
    if (pyobjects) {
      size_t n = bufsize / sizeof(PyObject*);
      if (bufsize != n * sizeof(PyObject*)) {
        icc << "MemoryRange is marked as containing PyObjects, but its "
               "size is " << bufsize << ", not a multiple of "
            << sizeof(PyObject*) << icc.end();
        return false;
      }
      PyObject** elements = static_cast<PyObject**>(bufdata);
      for (size_t i = 0; i < n; ++i) {
        if (elements[i] == nullptr) {
          icc << "Element " << i << " in pyobjects MemoryRange is NULL"
              << icc.end();
          return false;
        }
        if (elements[i]->ob_refcnt <= 0) {
          icc << "Reference count on PyObject at index " << i << " in "
                 "MemoryRange is "
              << static_cast<int64_t>(elements[i]->ob_refcnt) << icc.end();
          return false;
        }
      }
    }
    return true;
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

  size_t MemoryMRI::memory_footprint() const {
    return sizeof(MemoryMRI) + bufsize;
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

  bool MemoryMRI::verify_integrity(IntegrityCheckContext& icc) const {
    bool ok = MemoryRangeImpl::verify_integrity(icc);
    if (!ok) return false;
    if (bufsize) {
      size_t actual_allocsize = malloc_size(bufdata);
      if (bufsize > actual_allocsize) {
        icc << "MemoryRange has bufsize = " << bufsize << ", while the internal"
               " buffer was allocated for " << actual_allocsize << " bytes only"
            << icc.end();
        return false;
      }
    }
    return true;
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
    resizable = false;
    writeable = false;
  }

  ExternalMRI::ExternalMRI(size_t n, void* ptr) : ExternalMRI(n, ptr, nullptr) {
    writeable = true;
  }

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




//==============================================================================
// ViewMRI
//==============================================================================

  ViewMRI::ViewMRI(size_t n, MemoryRange& src, size_t offs)
    : source(src), offset(offs)
  {
    xassert(offs + n <= src.size());
    bufdata = const_cast<void*>(src.rptr(offs));
    bufsize = n;
    resizable = false;
    writeable = src.is_writeable();
  }

  size_t ViewMRI::memory_footprint() const {
    return sizeof(ViewMRI) + bufsize;
  }

  void ViewMRI::resize(size_t) { throw Error(); }

  bool ViewMRI::verify_integrity(IntegrityCheckContext& icc) const {
    bool ok = MemoryRangeImpl::verify_integrity(icc);
    if (!ok) return false;
    if (resizable) {
      icc << "View MemoryRange cannot be marked as resizable" << icc.end();
      return false;
    }

    return true;
  }




//==============================================================================
// ViewedMRI
//==============================================================================

  /*
  ViewedMRI::ViewedMRI(MemoryRange* src, MemoryRangeImpl* impl) {
    source_memoryrange = src;
    original_impl = impl;
    bufdata = original_impl->bufdata;
    bufsize = original_impl->bufsize;
    pyobjects = original_impl->pyobjects;
    writeable = false;
    resizable = false;
    view_refcount = 0;
  }

  ViewedMRI::~ViewedMRI() {
    if (original_impl) original_impl->release();
  }

  void ViewedMRI::release() {
    if (--refcount == 0) {
      source_memoryrange = nullptr;
      if (view_refcount == 0) {
        delete this;
      }
    }
  }

  void ViewedMRI::release_view() {
    if (--view_refcount == 0) {
      if (refcount != 0) {
        // TODO: restore
        // source_memoryrange->impl = original_impl;
        original_impl = nullptr;
      }
      delete this;
    }
  }

  void MemoryRange::convert_to_viewed() {
    if (dynamic_cast<ViewedMRI*>(impl)) return;
    if (impl->refcount != 1) {
      // Possibly we could materialize the MemoryRange instead
      throw Error() << "Refcount for MemoryRange is > 1, cannot apply view";
    }
    impl = new ViewedMRI(this, this->impl);
  }
  */




//==============================================================================
// MmapMRI
//==============================================================================

  MmapMRI::MmapMRI(const std::string& path)
    : MmapMRI(0, path, -1, false) {}

  MmapMRI::MmapMRI(size_t n, const std::string& path, int fileno)
    : MmapMRI(n, path, fileno, true) {}

  MmapMRI::MmapMRI(size_t n, const std::string& path, int fileno, bool create)
    : filename(path), fd(fileno), mapped(false)
  {
    bufdata = nullptr;
    bufsize = n;
    writeable = create;
    resizable = create;
    temporary_file = create;
  }

  MmapMRI::~MmapMRI() {
    memunmap();
    if (temporary_file) {
      File::remove(filename);
    }
  }

  void* MmapMRI::ptr() const {
    const_cast<MmapMRI*>(this)->memmap();
    return bufdata;
  }

  void MmapMRI::resize(size_t n) {
    memunmap();
    File file(filename, File::READWRITE);
    file.resize(n);
    memmap();
  }

  size_t MmapMRI::memory_footprint() const {
    return sizeof(MmapMRI) + filename.size() + (mapped? bufsize : 0);
  }

  void MmapMRI::memmap() {
    if (mapped) return;
    // Place a mutex lock to prevent multiple threads from trying to perform
    // memory-mapping of different files (or same file) in parallel. If multiple
    // threads called this method at the same time, then only one will proceed,
    // while all others will wait until the lock is released, and then exit
    // because flag `mapped` will now be true.
    #pragma clang diagnostic ignored "-Wexit-time-destructors"
    static std::mutex mmp_mutex;
    std::lock_guard<std::mutex> lock(mmp_mutex);

    bool create = temporary_file;
    size_t n = bufsize;

    File file(filename, create? File::CREATE : File::READ, fd);
    file.assert_is_not_dir();
    if (create) {
      file.resize(n);
    }
    size_t filesize = file.size();
    if (filesize == 0) {
      // Cannot memory-map 0-bytes file. However we shouldn't really need to:
      // if memory size is 0 then mmp can be NULL as nobody is going to read
      // from it anyways.
      bufsize = 0;
      mapped = true;
      return;
    }
    bufsize = filesize + (create? 0 : n);

    // Memory-map the file.
    // In "open" mode if `n` is non-zero, then we will be opening a buffer
    // with larger size than the actual file size. Also, the file is opened in
    // "private, read-write" mode -- meaning that the user can write to that
    // buffer if needed. From the man pages of `mmap`:
    //
    // | MAP_SHARED
    // |   Share this mapping. Updates to the mapping are visible to other
    // |   processes that map this file, and are carried through to the
    // |   underlying file. The file may not actually be updated until msync(2)
    // |   or munmap() is called.
    // | MAP_PRIVATE
    // |   Create a private copy-on-write mapping.  Updates to the mapping
    // |   are not carried through to the underlying file.
    // | MAP_NORESERVE
    // |   Do not reserve swap space for this mapping.  When swap space is
    // |   reserved, one has the guarantee that it is possible to modify the
    // |   mapping.  When swap space is not reserved one might get SIGSEGV
    // |   upon a write if no physical memory is available.
    //
    int attempts = 3;
    while (attempts--) {
      int flags = create? MAP_SHARED : MAP_PRIVATE|MAP_NORESERVE;
      bufdata = mmap(/* address = */ nullptr,
                     /* length = */ bufsize,
                     /* protection = */ PROT_WRITE|PROT_READ,
                     /* flags = */ flags,
                     /* fd = */ file.descriptor(),
                     /* offset = */ 0);
      if (bufdata == MAP_FAILED) {
        if (errno == 12) {  // release some memory and try again
          MemoryMapManager::get()->freeup_memory();
          continue;
        }
        // Exception is thrown from the constructor -> the base class'
        // destructor will be called, which checks that `bufdata` is null.
        bufdata = nullptr;
        throw RuntimeError() << "Memory-map failed for file " << file.cname()
                             << " of size " << filesize
                             << " +" << bufsize - filesize << Errno;
      } else {
        MemoryMapManager::get()->add_entry(this, bufsize);
        break;
      }
    }
    mapped = true;
  }


  void MmapMRI::memunmap() {
    if (!bufdata) return;
    int ret = munmap(bufdata, bufsize);
    if (ret) {
      // Cannot throw exceptions from a destructor, so just print a message
      printf("Error unmapping the view of file: [errno %d] %s. Resources may "
             "have not been freed properly.", errno, std::strerror(errno));
    }
    bufdata = nullptr;
    mapped = false;
    bufsize = 0;
    if (mmm_index) {
      MemoryMapManager::get()->del_entry(mmm_index);
      mmm_index = 0;
    }
  }


  size_t MmapMRI::size() const {
    if (mapped) {
      return bufsize;
    } else {
      bool create = temporary_file;
      size_t filesize = File::asize(filename);
      size_t extra = create? 0 : bufsize;
      return filesize==0? 0 : filesize + extra;
    }
  }


  void MmapMRI::save_entry_index(size_t i) {
    mmm_index = i;
  }

  void MmapMRI::evict() {
    memunmap();
  }



//==============================================================================
// Template instantiations
//==============================================================================

  template int32_t MemoryRange::get_element(int64_t) const;
  template int64_t MemoryRange::get_element(int64_t) const;
  template void MemoryRange::set_element(int64_t, int32_t);
  template void MemoryRange::set_element(int64_t, int64_t);
  template void MemoryRange::set_element(int64_t, PyObject*);
