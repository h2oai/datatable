//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "memrange.h"
#include <algorithm>           // std::min
#include <cerrno>              // errno
#include <mutex>               // std::mutex, std::lock_guard
#ifndef _WIN32
#include <sys/mman.h>          // mmap, munmap
#endif
#include "mmm.h"               // MemoryMapWorker, MemoryMapManager
#include "utils.h"             // malloc_size
#include "utils/alloc.h"       // dt::malloc, dt::realloc
#include "utils/exceptions.h"  // ValueError, MemoryError



//==============================================================================
// Implementation classes
//==============================================================================

  class BaseMRI {
    public:
      void*  bufdata;
      size_t bufsize;
      bool   pyobjects;
      bool   writable;
      bool   resizable;
      int64_t: 40;

    public:
      BaseMRI();
      virtual ~BaseMRI();
      void clear_pyobjects();

      virtual void resize(size_t) {}
      virtual size_t size() const { return bufsize; }
      virtual void* ptr() const { return bufdata; }
      virtual size_t memory_footprint() const = 0;
      virtual const char* name() const = 0;
      virtual void verify_integrity() const;
  };


  struct MemoryRange::internal {
    std::unique_ptr<BaseMRI> impl;

    internal(BaseMRI* _impl) : impl(std::move(_impl)) {}
  };


  class MemoryMRI : public BaseMRI {
    public:
      MemoryMRI(size_t n);
      MemoryMRI(size_t n, void* ptr);
      ~MemoryMRI() override;

      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "ram"; }
      void verify_integrity() const override;
  };


  class ExternalMRI : public BaseMRI {
    private:
      Py_buffer* pybufinfo;

    public:
      ExternalMRI(size_t n, const void* ptr);
      ExternalMRI(size_t n, const void* ptr, Py_buffer* pybuf);
      ExternalMRI(const char* str);
      ~ExternalMRI() override;

      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "ext"; }
  };


  // ViewMRI represents a memory range which is a part of a larger memory
  // region controlled by `base` ViewedMRI.
  //
  // Typical use-case: memory-map a file, then carve out various regions of
  // that file as separate MemoryRange objects for each column. Another example:
  // when converting to Numpy, allocate a large contiguous chunk of memory,
  // then split it into separate memory buffers for each column, and cast the
  // existing Frame into those prepared column buffers.
  //
  // ViewMRI exists in tandem with ViewedMRI (which replaces the `impl` of the
  // object being viewed). This mechanism is needed in order to keep the source
  // MemoryRegion impl alive even if its owner went out of scope. The mechanism
  // is the following:
  // 1) When a view onto a MemoryRange is created, its `impl` is replaced with
  //    a `ViewedMRI` object, which holds the original impl, and carries a
  //    `shared_ptr<internal>` reference to the source MemoryRange's `o`. This
  //    prevents the ViewedMRI `impl` from being deleted even if the original
  //    MemoryRange object goes out of scope.
  // 2) Each view (ViewMRI) carries a reference `ViewedMRI* base` to the object
  //    being viewed. The `ViewedMRI` in turn contains a `refcount` to keep
  //    track of how many views are still using it.
  // 3) When ViewedMRI's `refounct` reaches 0, it means there are no longer any
  //    views onto the original MemoryRange object, and the original `impl` can
  //    be restored.
  //
  class ViewMRI : public BaseMRI {
    private:
      size_t offset;
      ViewedMRI* base;

    public:
      ViewMRI(size_t n, MemoryRange& src, size_t offset);
      virtual ~ViewMRI() override;

      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "view"; }
      void verify_integrity() const override;
  };


  class ViewedMRI : public BaseMRI {
    private:
      std::shared_ptr<MemoryRange::internal> parent;
      BaseMRI* original_impl;
      size_t refcount;

    public:
      static ViewedMRI* acquire_viewed(MemoryRange& src);
      void release();

      bool is_writable() const;
      size_t memory_footprint() const override;
      const char* name() const override { return "viewed"; }

    private:
      ViewedMRI(MemoryRange& src);
    };


  class MmapMRI : public BaseMRI, MemoryMapWorker {
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
      virtual ~MmapMRI() override;

      void* ptr() const override;
      size_t size() const override;
      void save_entry_index(size_t i) override;
      void evict() override;
      void resize(size_t n) override;
      size_t memory_footprint() const override;
      const char* name() const override { return "mmap"; }
      void verify_integrity() const override;

    protected:
      virtual void memmap();
      void memunmap();
  };


  class OvermapMRI : public MmapMRI {
    private:
      void* xbuf;
      size_t xbuf_size;

    public:
      OvermapMRI(const std::string& path, size_t n, int fd = -1);
      ~OvermapMRI() override;

      virtual size_t memory_footprint() const override;
      const char* name() const override { return "omap"; }

    protected:
      void memmap() override;
  };




//==============================================================================
// Main `MemoryRange` class
//==============================================================================

  //---- Constructors ----------------------------

  MemoryRange::MemoryRange(BaseMRI* impl) {
    o = std::make_shared<internal>(impl);
  }

  MemoryRange::MemoryRange() : MemoryRange(new MemoryMRI(0)) {}

  MemoryRange MemoryRange::mem(size_t n) {
    return MemoryRange(new MemoryMRI(n));
  }

  MemoryRange MemoryRange::mem(int64_t n) {
    return MemoryRange(new MemoryMRI(static_cast<size_t>(n)));
  }

  MemoryRange MemoryRange::acquire(void* ptr, size_t n) {
    return MemoryRange(new MemoryMRI(n, ptr));
  }

  MemoryRange MemoryRange::external(const void* ptr, size_t n) {
    return MemoryRange(new ExternalMRI(n, ptr));
  }

  MemoryRange MemoryRange::external(const void* ptr, size_t n, Py_buffer* pb) {
    return MemoryRange(new ExternalMRI(n, ptr, pb));
  }

  MemoryRange MemoryRange::view(MemoryRange& src, size_t n, size_t offset) {
    return MemoryRange(new ViewMRI(n, src, offset));
  }

  MemoryRange MemoryRange::mmap(const std::string& path) {
    return MemoryRange(new MmapMRI(path));
  }

  MemoryRange MemoryRange::mmap(const std::string& path, size_t n, int fd) {
    return MemoryRange(new MmapMRI(n, path, fd));
  }

  MemoryRange MemoryRange::overmap(const std::string& path, size_t extra_n,
                                   int fd)
  {
    return MemoryRange(new OvermapMRI(path, extra_n, fd));
  }


  //---- Basic properties ------------------------

  MemoryRange::operator bool() const {
    return (o->impl->size() != 0);
  }

  bool MemoryRange::is_writable() const {
    return (o.use_count() == 1) && o->impl->writable;
  }

  bool MemoryRange::is_resizable() const {
    return (o.use_count() == 1) && o->impl->resizable;
  }

  bool MemoryRange::is_pyobjects() const {
    return o->impl->pyobjects;
  }

  size_t MemoryRange::size() const {
    return o->impl->size();
  }

  size_t MemoryRange::memory_footprint() const {
    return sizeof(MemoryRange) + sizeof(internal) + o->impl->memory_footprint();
  }


  //---- Main data accessors ---------------------

  const void* MemoryRange::rptr() const {
    return o->impl->ptr();
  }

  const void* MemoryRange::rptr(size_t offset) const {
    const char* ptr0 = static_cast<const char*>(rptr());
    return static_cast<const void*>(ptr0 + offset);
  }

  void* MemoryRange::wptr() {
    if (!is_writable()) materialize();
    return o->impl->ptr();
  }

  void* MemoryRange::wptr(size_t offset) {
    char* ptr0 = static_cast<char*>(wptr());
    return static_cast<void*>(ptr0 + offset);
  }

  void* MemoryRange::xptr() const {
    if (!is_writable()) {
      throw RuntimeError() << "Cannot write into this MemoryRange object: "
        "refcount=" << o.use_count() << ", writable=" << o->impl->writable;
    }
    return o->impl->ptr();
  }

  void* MemoryRange::xptr(size_t offset) const {
    return static_cast<void*>(static_cast<char*>(xptr()) + offset);
  }


  //---- MemoryRange manipulators ----------------

  MemoryRange& MemoryRange::set_pyobjects(bool clear_data) {
    size_t n = o->impl->size() / sizeof(PyObject*);
    xassert(n * sizeof(PyObject*) == o->impl->size());
    xassert(this->is_writable());
    if (clear_data) {
      PyObject** data = static_cast<PyObject**>(o->impl->ptr());
      for (size_t i = 0; i < n; ++i) {
        data[i] = Py_None;
      }
      Py_None->ob_refcnt += n;
    }
    o->impl->pyobjects = true;
    return *this;
  }

  MemoryRange& MemoryRange::resize(size_t newsize, bool keep_data) {
    size_t oldsize = o->impl->size();
    if (newsize != oldsize) {
      if (is_resizable()) {
        if (o->impl->pyobjects) {
          size_t n_old = oldsize / sizeof(PyObject*);
          size_t n_new = newsize / sizeof(PyObject*);
          if (n_new < n_old) {
            PyObject** data = static_cast<PyObject**>(o->impl->ptr());
            for (size_t i = n_new; i < n_old; ++i) Py_DECREF(data[i]);
          }
          o->impl->resize(newsize);
          if (n_new > n_old) {
            PyObject** data = static_cast<PyObject**>(o->impl->ptr());
            for (size_t i = n_old; i < n_new; ++i) data[i] = Py_None;
            Py_None->ob_refcnt += n_new - n_old;
          }
        } else {
          o->impl->resize(newsize);
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
    auto wb = WritableBuffer::create_target(path, o->impl->size(), strategy);
    wb->write(o->impl->size(), o->impl->ptr());
  }


  PyObject* MemoryRange::pyrepr() const {
    return PyUnicode_FromFormat("<MemoryRange:%s %p+%zu (ref=%zu)>",
                                o->impl->name(), o->impl->ptr(), o->impl->size(),
                                o.use_count());
  }

  void MemoryRange::verify_integrity() const {
    if (!o || !o->impl) {
      throw AssertionError() << "NULL implementation object in MemoryRange";
    }
    o->impl->verify_integrity();
  }

  void MemoryRange::materialize() {
    size_t s = o->impl->size();
    materialize(s, s);
  }

  void MemoryRange::materialize(size_t newsize, size_t copysize) {
    xassert(newsize >= copysize);
    MemoryMRI* newimpl = new MemoryMRI(newsize);
    if (copysize) {
      std::memcpy(newimpl->ptr(), o->impl->ptr(), copysize);
    }
    if (o->impl->pyobjects) {
      newimpl->pyobjects = true;
      PyObject** newdata = static_cast<PyObject**>(newimpl->ptr());
      size_t n_new = newsize / sizeof(PyObject*);
      size_t n_copy = copysize / sizeof(PyObject*);
      size_t i = 0;
      for (; i < n_copy; ++i) Py_INCREF(newdata[i]);
      for (; i < n_new; ++i) newdata[i] = Py_None;
      Py_None->ob_refcnt += n_new - n_copy;
    }
    o = std::make_shared<internal>(std::move(newimpl));
  }


  //---- Element getters/setters -----------------

  static void _oob_check(size_t i, size_t size, size_t elemsize) {
    if ((i + 1) * elemsize > size) {
      throw ValueError() << "Index " << i << " is out of bounds for a memory "
        "region of size " << size << " viewed as an array of elements of size "
        << elemsize;
    }
  }

  template <typename T>
  T MemoryRange::get_element(size_t i) const {
    _oob_check(i, size(), sizeof(T));
    const T* data = static_cast<const T*>(this->rptr());
    return data[i];
  }

  template <>
  void MemoryRange::set_element(size_t i, PyObject* value) {
    _oob_check(i, size(), sizeof(PyObject*));
    xassert(this->is_pyobjects());
    PyObject** data = static_cast<PyObject**>(this->wptr());
    Py_DECREF(data[i]);
    data[i] = value;
  }

  template <typename T>
  void MemoryRange::set_element(size_t i, T value) {
    _oob_check(i, size(), sizeof(T));
    T* data = static_cast<T*>(this->wptr());
    data[i] = value;
  }




//==============================================================================
// BaseMRI
//==============================================================================

  BaseMRI::BaseMRI()
    : bufdata(nullptr),
      bufsize(0),
      pyobjects(false),
      writable(true),
      resizable(true) {}

  BaseMRI::~BaseMRI() {
    if (pyobjects) {
      printf("pyobjects not properly cleared\n");
    }
  }

  // This method must be called by the destructors of the derived classes. The
  // reason this code is not in the ~BaseMRI is because it is the derived
  // classes who handle freeing of `bufdata`, and clearing PyObjects must
  // happen before that.
  //
  void BaseMRI::clear_pyobjects() {
    // If memory buffer contains `PyObject*`s, then they must be DECREFed
    // before being deleted.
    if (pyobjects) {
      PyObject** items = static_cast<PyObject**>(bufdata);
      size_t n = bufsize / sizeof(PyObject*);
      for (size_t i = 0; i < n; ++i) {
        Py_DECREF(items[i]);
      }
      pyobjects = false;
    }
  }

  void BaseMRI::verify_integrity() const {
    if (!bufdata && bufsize) {
      throw AssertionError()
          << "MemoryRange has bufdata = NULL but size = " << bufsize;
    }
    if (bufdata && !bufsize) {
      throw AssertionError()
          << "MemoryRange has bufdata = " << bufdata << " but size = 0";
    }
    if (resizable && !writable) {
      throw AssertionError() << "MemoryRange is resizable but not writable";
    }
    if (pyobjects) {
      size_t n = bufsize / sizeof(PyObject*);
      if (bufsize != n * sizeof(PyObject*)) {
        throw AssertionError()
            << "MemoryRange is marked as containing PyObjects, but its size is "
            << bufsize << ", not a multiple of " << sizeof(PyObject*);
      }
      PyObject** elements = static_cast<PyObject**>(bufdata);
      for (size_t i = 0; i < n; ++i) {
        if (elements[i] == nullptr) {
          throw AssertionError()
              << "Element " << i << " in pyobjects MemoryRange is NULL";
        }
        if (elements[i]->ob_refcnt <= 0) {
          throw AssertionError()
              << "Reference count on PyObject at index " << i
              << " in MemoryRange is " << elements[i]->ob_refcnt;
        }
      }
    }
  }




//==============================================================================
// MemoryMRI
//==============================================================================

  MemoryMRI::MemoryMRI(size_t n) {
    bufsize = n;
    bufdata = dt::malloc<void>(n);
  }

  MemoryMRI::MemoryMRI(size_t n, void* ptr) {
    if (n) {
      if (!ptr) throw ValueError() << "Unallocated memory region provided";
      bufsize = n;
      bufdata = ptr;
    }
  }

  MemoryMRI::~MemoryMRI() {
    clear_pyobjects();
    std::free(bufdata);
  }

  size_t MemoryMRI::memory_footprint() const {
    return sizeof(MemoryMRI) + bufsize;
  }

  void MemoryMRI::resize(size_t n) {
    if (n == bufsize) return;
    bufdata = dt::realloc(bufdata, n);
    bufsize = n;
  }

  void MemoryMRI::verify_integrity() const {
    BaseMRI::verify_integrity();
    if (bufsize) {
      size_t actual_allocsize = malloc_size(bufdata);
      if (bufsize > actual_allocsize) {
        throw AssertionError()
            << "MemoryRange has bufsize = " << bufsize
            << ", while the internal buffer was allocated for "
            << actual_allocsize << " bytes only";
      }
    }
  }




//==============================================================================
// ExternalMRI
//==============================================================================

  ExternalMRI::ExternalMRI(size_t size, const void* ptr, Py_buffer* pybuf) {
    if (!ptr && size > 0) {
      throw RuntimeError() << "Unallocated buffer supplied to the ExternalMRI "
          "constructor. Expected memory region of size " << size;
    }
    bufdata = const_cast<void*>(ptr);
    bufsize = size;
    pybufinfo = pybuf;
    resizable = false;
    writable = false;
  }

  ExternalMRI::ExternalMRI(size_t n, const void* ptr)
    : ExternalMRI(n, ptr, nullptr)
  {
    writable = true;
  }

  ExternalMRI::ExternalMRI(const char* str)
      : ExternalMRI(strlen(str) + 1, str, nullptr) {}

  ExternalMRI::~ExternalMRI() {
    // If the buffer contained pyobjects, leave them as-is and do not attempt
    // to DECREF (this is up to the external owner).
    pyobjects = false;
    if (pybufinfo) {
      PyBuffer_Release(pybufinfo);
    }
  }

  void ExternalMRI::resize(size_t) {
    throw Error() << "Unable to resize an ExternalMRI buffer";
  }

  size_t ExternalMRI::memory_footprint() const {
    return sizeof(ExternalMRI) + bufsize + (pybufinfo? sizeof(Py_buffer) : 0);
  }




//==============================================================================
// ViewMRI
//==============================================================================

  ViewMRI::ViewMRI(size_t n, MemoryRange& src, size_t offs) {
    xassert(offs + n <= src.size());
    base = ViewedMRI::acquire_viewed(src);
    offset = offs;
    bufdata = const_cast<void*>(src.rptr(offs));
    bufsize = n;
    resizable = false;
    writable = base->is_writable();
    pyobjects = src.is_pyobjects();
  }

  ViewMRI::~ViewMRI() {
    base->release();
    pyobjects = false;
  }

  size_t ViewMRI::memory_footprint() const {
    return sizeof(ViewMRI) + bufsize;
  }

  void ViewMRI::resize(size_t) {
    throw RuntimeError() << "ViewMRI cannot be resized";
  }

  void ViewMRI::verify_integrity() const {
    BaseMRI::verify_integrity();
    if (resizable) {
      throw AssertionError() << "ViewMRI cannot be marked as resizable";
    }
    void* base_ptr = static_cast<void*>(
                        static_cast<char*>(base->bufdata) + offset);
    if (base_ptr != bufdata) {
      throw AssertionError()
          << "Invalid data pointer in View MemoryRange: should be "
          << base_ptr << " but actual pointer is " << bufdata;
    }
  }




//==============================================================================
// ViewedMRI
//==============================================================================

  ViewedMRI::ViewedMRI(MemoryRange& src) {
    BaseMRI* implptr = src.o->impl.release();
    src.o->impl.reset(this);
    parent = src.o;  // copy std::shared_ptr
    original_impl = implptr;
    refcount = 0;
    bufdata = implptr->bufdata;
    bufsize = implptr->bufsize;
    pyobjects = implptr->pyobjects;
    writable = false;
    resizable = false;
  }

  ViewedMRI* ViewedMRI::acquire_viewed(MemoryRange& src) {
    BaseMRI* implptr = src.o->impl.get();
    ViewedMRI* viewedptr = dynamic_cast<ViewedMRI*>(implptr);
    if (!viewedptr) {
      viewedptr = new ViewedMRI(src);
    }
    viewedptr->refcount++;
    return viewedptr;
  }

  void ViewedMRI::release() {
    if (--refcount == 0) {
      parent->impl.release();
      parent->impl.reset(original_impl);
      delete this;
    }
  }

  bool ViewedMRI::is_writable() const {
    return original_impl->writable;
  }

  size_t ViewedMRI::memory_footprint() const {
    return 0;
  }



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
    writable = create;
    resizable = create;
    temporary_file = create;
    mmm_index = 0;
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
    #ifdef _WIN32
      throw RuntimeError() << "Memory-mapping not supported on Windows yet";

    #else
      // Place a mutex lock to prevent multiple threads from trying to perform
      // memory-mapping of different files (or same file) in parallel. If
      // multiple threads called this method at the same time, then only one
      // will proceed, while all others will wait until the lock is released,
      // and then exit because flag `mapped` will now be true.
      static std::mutex mmp_mutex;
      std::lock_guard<std::mutex> _(mmp_mutex);
      if (mapped) return;

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
        bufdata = nullptr;
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
      // |   underlying file. The file may not actually be updated until
      // |   msync(2) or munmap() is called.
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
          bufdata = nullptr;
          if (errno == 12) {  // release some memory and try again
            MemoryMapManager::get()->freeup_memory();
            if (attempts) {
              errno = 0;
              continue;
            }
          }
          // Exception is thrown from the constructor -> the base class'
          // destructor will be called, which checks that `bufdata` is null.
          throw RuntimeError() << "Memory-map failed for file " << file.cname()
                               << " of size " << filesize
                               << " +" << bufsize - filesize << Errno;
        } else {
          MemoryMapManager::get()->add_entry(this, bufsize);
          break;
        }
      }
      mapped = true;
      xassert(mmm_index);
    #endif
  }


  void MmapMRI::memunmap() {
    if (!mapped) return;
    #ifdef _WIN32
    #else
      if (bufdata) {
        int ret = munmap(bufdata, bufsize);
        if (ret) {
          // Cannot throw exceptions from a destructor, so just print a message
          printf("Error unmapping the view of file: [errno %d] %s. Resources "
                 "may have not been freed properly.",
                 errno, std::strerror(errno));
        }
        bufdata = nullptr;
      }
      mapped = false;
      bufsize = 0;
      if (mmm_index) {
        MemoryMapManager::get()->del_entry(mmm_index);
        mmm_index = 0;
      }
    #endif
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
    xassert(MemoryMapManager::get()->check_entry(mmm_index, this));
  }

  void MmapMRI::evict() {
    mmm_index = 0;  // prevent from sending del_entry() signal back
    memunmap();
    xassert(!mapped && !mmm_index);
  }

  void MmapMRI::verify_integrity() const {
    BaseMRI::verify_integrity();
    if (mapped) {
      if (!MemoryMapManager::get()->check_entry(mmm_index, this)) {
        throw AssertionError()
            << "Mmap MemoryRange is not properly registered with the "
               "MemoryMapManager: mmm_index = " << mmm_index;
      }
      if (bufsize == 0 && bufdata) {
        throw AssertionError()
            << "Mmap MemoryRange has size = 0 but data pointer is: " << bufdata;
      }
      if (bufsize && !bufdata) {
        throw AssertionError()
            << "Mmap MemoryRange has size = " << bufsize
            << " and marked as mapped, however its data pointer is NULL";
      }
    } else {
      if (mmm_index) {
        throw AssertionError()
            << "Mmap MemoryRange is not mapped but its mmm_index = "
            << mmm_index;
      }
      if (bufsize || bufdata) {
        throw AssertionError()
            << "Mmap MemoryRange is not mapped but its size = " << bufsize
            << " and data pointer = " << bufdata;
      }
    }
  }



//==============================================================================
// OvermapMRI
//==============================================================================

  OvermapMRI::OvermapMRI(const std::string& path, size_t xn, int fd)
      : MmapMRI(xn, path, fd, false), xbuf(nullptr), xbuf_size(xn)
  {
    writable = true;
  }


  void OvermapMRI::memmap() {
    MmapMRI::memmap();
    if (xbuf_size == 0) return;
    if (!bufdata) return;
    #ifdef _WIN32
      throw RuntimeError() << "Memory-mapping not supported on Windows yet";

    #else
      // The parent's constructor has opened a memory-mapped region of size
      // `filesize + xn`. This, however, is not always enough:
      // | A file is mapped in multiples of the page size. For a file that is
      // | not a multiple of the page size, the remaining memory is 0ed when
      // | mapped, and writes to that region are not written out to the file.
      //
      // Thus, when `filesize` is *not* a multiple of pagesize, then the
      // memory mapping will have some writable "scratch" space at the end,
      // filled with '\0' bytes. We check -- if this space is large enough to
      // hold `xn` bytes, then don't do anything extra. If not (for example
      // when `filesize` is an exact multiple of `pagesize`), then attempt to
      // read/write past physical end of file wil fail with a BUS error --
      // despite the fact that the map was overallocated for the extra `xn`
      // bytes:
      // | Use of a mapped region can result in these signals:
      // | SIGBUS:
      // |   Attempted access to a portion of the buffer that does not
      // |   correspond to the file (for example, beyond the end of the file)
      //
      // In order to circumvent this, we allocate a new memory-mapped region of
      // size `xn` and placed at address `buf + filesize`. In theory, this
      // should always succeed because we over-allocated `buf` by `xn` bytes;
      // and even though those extra bytes are not readable/writable, at least
      // there is a guarantee that it is not occupied by anyone else. Now,
      // `mmap()` documentation explicitly allows to declare mappings that
      // overlap each other:
      // | MAP_ANONYMOUS:
      // |   The mapping is not backed by any file; its contents are
      // |   initialized to zero. The fd argument is ignored.
      // | MAP_FIXED
      // |   Don't interpret addr as a hint: place the mapping at exactly
      // |   that address.  `addr` must be a multiple of the page size. If
      // |   the memory region specified by addr and len overlaps pages of
      // |   any existing mapping(s), then the overlapped part of the existing
      // |   mapping(s) will be discarded.
      //
      size_t xn = xbuf_size;
      size_t pagesize = static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
      size_t filesize = size() - xn;
      // How much to add to filesize to align it to a page boundary
      size_t gapsize = (pagesize - filesize%pagesize) % pagesize;
      if (xn > gapsize) {
        void* target = static_cast<void*>(
                          static_cast<char*>(bufdata) + filesize + gapsize);
        xbuf_size = xn - gapsize;
        xbuf = mmap(/* address = */ target,
                    /* size = */ xbuf_size,
                    /* protection = */ PROT_WRITE|PROT_READ,
                    /* flags = */ MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED,
                    /* file descriptor, ignored */ -1,
                    /* offset, ignored */ 0);
        if (xbuf == MAP_FAILED) {
          throw RuntimeError() << "Cannot allocate additional " << xbuf_size
                               << " bytes at address " << target << ": "
                               << Errno;
        }
      }
    #endif
  }


  OvermapMRI::~OvermapMRI() {
    if (!xbuf) return;
    #ifndef _WIN32
      int ret = munmap(xbuf, xbuf_size);
      if (ret) {
        printf("Cannot unmap extra memory %p: [errno %d] %s",
               xbuf, errno, std::strerror(errno));
      }
    #endif
  }


  size_t OvermapMRI::memory_footprint() const {
    return MmapMRI::memory_footprint() - sizeof(MmapMRI) +
           xbuf_size + sizeof(OvermapMRI);
  }

  // Check that resizable = false



//==============================================================================
// Template instantiations
//==============================================================================

  template int32_t MemoryRange::get_element(size_t) const;
  template int64_t MemoryRange::get_element(size_t) const;
  template uint32_t MemoryRange::get_element(size_t) const;
  template uint64_t MemoryRange::get_element(size_t) const;
  template void MemoryRange::set_element(size_t, char);
  template void MemoryRange::set_element(size_t, int32_t);
  template void MemoryRange::set_element(size_t, int64_t);
  template void MemoryRange::set_element(size_t, uint32_t);
  template void MemoryRange::set_element(size_t, uint64_t);
