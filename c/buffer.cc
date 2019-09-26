//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <algorithm>           // std::min
#include <cerrno>              // errno
#include <mutex>               // std::mutex, std::lock_guard
#include "utils/alloc.h"       // dt::malloc, dt::realloc
#include "utils/exceptions.h"  // ValueError, MemoryError
#include "utils/macros.h"
#include "utils/misc.h"        // malloc_size
#include "datatablemodule.h"   // TRACK, UNTRACK, IS_TRACKED
#include "buffer.h"
#include "mmm.h"               // MemoryMapWorker, MemoryMapManager

#if !DT_OS_WINDOWS
  #include <sys/mman.h>        // mmap, munmap
#endif


//------------------------------------------------------------------------------
// BufferImpl
//------------------------------------------------------------------------------

/**
  * Abstract implementation for the MemoryRange object.
  *
  * BufferImpl represents a contiguous chunk of memory, stored as the
  * `data_` pointer + `size_`. This base class does not own the data
  * pointer, it is up to the derived class to manage the memory
  * ownership, and to free the resources when needed.
  *
  * The class uses the reference-counting semantics, and thus acts
  * as a self-owning object. The refcount is set to 1 when the object
  * is created; all subsequent copies of the pointer must be done via
  * the `acquire()` call, which must be paired with the corresponding
  * `release()` call when the pointer is no longer needed. The
  * object will self-destruct when its refcount reaches 0.
  *
  * The BufferImpl may be marked as containing PyObjects. They will
  * be incref'd when copied, and decref'd when the buffer is resized
  * or destructed.
  *
  * The BufferImpl also carries flags that indicate whether the object
  * is writable / resizable. Resizing will also be forbidden if there
  * are multiple users sharing the same pointer (as indicated by the
  * refcount). Writing will also be forbidden for multiple users,
  * unless they called `acquire_shared()` to indicate that copy-on-
  * -write semantics should be suspended.
  */
class BufferImpl
{
  friend class MemoryRange;
  protected:
    void*  data_;
    size_t size_;
    size_t refcount_;
    uint32_t nshared_;
    bool   contains_pyobjects_;
    bool   writable_;
    bool   resizable_;
    int : 8;

  //------------------------------------
  // Constructors
  //------------------------------------
  public:
    BufferImpl()
      : data_(nullptr),
        size_(0),
        refcount_(1),
        nshared_(0),
        contains_pyobjects_(false),
        writable_(true),
        resizable_(true) {}

    virtual ~BufferImpl() {
      wassert(!contains_pyobjects_);
    }

    // If memory buffer contains `PyObject*`s, then they must be
    // DECREFed before being deleted.
    //
    // This method must be called by the destructors of the derived
    // classes. The reason this code is not in the ~BufferImpl is
    // because it is the derived classes who handle freeing
    // `data_`, and decrefing PyObjects must happen before that.
    //
    void clear_pyobjects() {
      if (!contains_pyobjects_) return;
      PyObject** items = static_cast<PyObject**>(data_);
      size_t n = size_ / sizeof(PyObject*);
      for (size_t i = 0; i < n; ++i) {
        Py_DECREF(items[i]);
      }
      contains_pyobjects_ = false;
    }


  //------------------------------------
  // Refcounting
  //------------------------------------
  // Call `acquire()` when storing a `BufferImpl*` pointer in a
  // variable. Call `release()` when that variable or its contents
  // are destroyed.
  //
  // The "shared" version of acquire/release calls allows the
  // object to be shared with another user: both owners of the
  // "shared" pointer are allowed to write into the data buffer
  // (however they still can't resize it).
  //
  public:
    inline BufferImpl* acquire() noexcept {
      refcount_++;
      return this;
    }

    inline void release() noexcept {
      refcount_--;
      if (refcount_ == 0) delete this;
    }

    inline BufferImpl* acquire_shared() noexcept {
      refcount_++;
      nshared_++;
      return this;
    }

    inline void release_shared() noexcept {
      refcount_--;
      nshared_--;
      if (refcount_ == 0) delete this;
    }


  //------------------------------------
  // Properties
  //------------------------------------
  public:
    // size() and data() are virtual to allow the children to fetch
    // the data pointer only when it is needed.
    virtual size_t size() const {
      return size_;
    }

    virtual void* data() const {
      return data_;
    }

    bool is_resizable() const noexcept {
      return resizable_ && (refcount_ == 1);
    }

    bool is_writable() const noexcept {
      return writable_ && (refcount_ - nshared_ == 1);
    }

    bool is_pyobjects() const noexcept {
      return contains_pyobjects_;
    }


  //------------------------------------
  // Methods
  //------------------------------------
  public:
    virtual void resize(size_t) {
      throw AssertionError() << "buffer cannot be resized";
    }

    virtual size_t memory_footprint() const = 0;

    virtual void verify_integrity() const {
      if (data_) { XAssert(size_ > 0); }
      else       { XAssert(size_ == 0); }
      if (resizable_) {
        XAssert(writable_);
      }
      if (contains_pyobjects_) {
        size_t n = size_ / sizeof(PyObject*);
        XAssert(size_ == n * sizeof(PyObject*));
        PyObject** elements = static_cast<PyObject**>(data_);
        for (size_t i = 0; i < n; ++i) {
          XAssert(elements[i] != nullptr);
          XAssert(elements[i]->ob_refcnt > 0);
        }
      }
    }
};




//------------------------------------------------------------------------------
// Memory_BufferImpl
//------------------------------------------------------------------------------

/**
  * Simple buffer that represents a piece of memory allocated via
  * dt::malloc(). The memory is owned by this class.
  */
class Memory_BufferImpl : public BufferImpl
{
  public:
    explicit Memory_BufferImpl(size_t n) {
      size_ = n;
      data_ = dt::malloc<void>(n);  // may throw
    }

    // Assumes ownership of pointer `ptr` (the pointer must be
    // deletable via `dt::free()`).
    Memory_BufferImpl(size_t n, void*&& ptr) {
      XAssert(ptr || n == 0);
      size_ = n;
      data_ = ptr;
    }

    ~Memory_BufferImpl() override {
      clear_pyobjects();
      dt::free(data_);
   }

    void resize(size_t n) override {
      if (n == size_) return;
      data_ = dt::realloc(data_, n);
      size_ = n;
    }

    size_t memory_footprint() const override {
      return sizeof(Memory_BufferImpl) + size_;
    }

    void verify_integrity() const override {
      BufferImpl::verify_integrity();
      if (size_) {
        size_t actual_allocsize = malloc_size(data_);
        XAssert(size_ <= actual_allocsize);
      }
    }
};




//------------------------------------------------------------------------------
// External_BufferImpl
//------------------------------------------------------------------------------

/**
  * This class represents a piece of memory owned by some external
  * entity. The lifetime of the memory region may be guarded by a
  * Py_buffer object. However, it is also possible to wrap a
  * completely unguarded memory range, in which cast it is the
  * responsibility of the user to ensure that the memory remains
  * valid during the lifetime of External_BufferImpl object.
  */
class External_BufferImpl : public BufferImpl
{
  private:
    Py_buffer* pybufinfo_;

  public:
    External_BufferImpl(size_t n, const void* ptr, Py_buffer* pybuf) {
      XAssert(ptr || n == 0);
      data_ = const_cast<void*>(ptr);
      size_ = n;
      pybufinfo_ = pybuf;
      resizable_ = false;
      writable_ = false;
    }

    External_BufferImpl(size_t n, const void* ptr)
      : External_BufferImpl(n, ptr, nullptr) {}

    External_BufferImpl(size_t n, void* ptr)
      : External_BufferImpl(n, ptr, nullptr) { writable_ = true; }

    ~External_BufferImpl() override {
      // If the buffer contained pyobjects, leave them as-is and
      // do not attempt to DECREF (since the memory is not freed).
      contains_pyobjects_ = false;
      if (pybufinfo_) {
        PyBuffer_Release(pybufinfo_);
      }
    }

    size_t memory_footprint() const override {
      // All memory is owned externally
      return sizeof(External_BufferImpl);
    }
};




//------------------------------------------------------------------------------
// View_BufferImpl
//------------------------------------------------------------------------------

/**
  * View_BufferImpl represents a buffer that is a "view" onto another
  * buffer `parent`.
  *
  * Typical use-case: memory-map a file, then carve out various
  * regions of that file as separate Buffer objects for each column.
  * Another example: when converting to Numpy, allocate a large
  * contiguous chunk of memory, then split it into separate buffers
  * for each column, and cast the existing Frame into those prepared
  * column buffers.
  */
class View_BufferImpl : public BufferImpl
{
  private:
    BufferImpl* parent_;
    size_t offset_;

  public:
    View_BufferImpl(BufferImpl* src, size_t n, size_t offs)
    {
      XAssert(offs + n <= src->size());
      parent_ = src->acquire_shared();
      offset_ = offs;
      data_ = static_cast<char*>(src->data()) + offset_;
      size_ = n;
      resizable_ = false;
      writable_ = src->is_writable();
      contains_pyobjects_ = src->is_pyobjects();
    }

    virtual ~View_BufferImpl() override {
      contains_pyobjects_ = false;
      parent_->release_shared();
    }

    size_t memory_footprint() const override {
      return sizeof(View_BufferImpl) + size_;
    }

    void verify_integrity() const override {
      BufferImpl::verify_integrity();
      XAssert(!resizable_);
      XAssert(data_ == static_cast<const char*>(parent_->data()) + offset_);
    }
};




//------------------------------------------------------------------------------
// Mmap_BufferImpl
//------------------------------------------------------------------------------

class Mmap_BufferImpl : public BufferImpl, MemoryMapWorker {
  private:
    const std::string filename_;
    size_t mmm_index_;
    int fd_;
    bool mapped_;
    bool temporary_file_;
    int : 16;

  //------------------------------------
  // Constructors
  //------------------------------------
  public:
    explicit Mmap_BufferImpl(const std::string& path)
      : Mmap_BufferImpl(0, path, -1, false) {}

    Mmap_BufferImpl(size_t n, const std::string& path, int fileno)
      : Mmap_BufferImpl(n, path, fileno, true) {}

    Mmap_BufferImpl(size_t n, const std::string& path, int fileno, bool create)
      : filename_(path), fd_(fileno), mapped_(false)
    {
      data_ = nullptr;
      size_ = n;
      writable_ = create;
      resizable_ = create;
      temporary_file_ = create;
      mmm_index_ = 0;
    }

    virtual ~Mmap_BufferImpl() override {
      memunmap();
      if (temporary_file_) {
        File::remove(filename_, /* except= */ false);
      }
    }


  //------------------------------------
  // Properties
  //------------------------------------
  public:
    void* data() const override {
      const_cast<Mmap_BufferImpl*>(this)->memmap();
      return data_;
    }

    size_t size() const override {
      if (mapped_) {
        return size_;
      } else {
        bool create = temporary_file_;
        size_t filesize = File::asize(filename_);
        size_t extra = create? 0 : size_;
        return filesize==0? 0 : filesize + extra;
      }
    }

    void resize(size_t n) override {
      memunmap();
      File file(filename_, File::READWRITE);
      file.resize(n);
      memmap();
    }

    size_t memory_footprint() const override {
      return sizeof(Mmap_BufferImpl) + filename_.size() + (mapped_? size_ : 0);
    }

    void verify_integrity() const override {
      BufferImpl::verify_integrity();
      if (mapped_) {
        XAssert(MemoryMapManager::get()->check_entry(mmm_index_, this));
      }
      else {
        XAssert(mmm_index_ == 0);
        XAssert(!size_ && !data_);
      }
    }


  //------------------------------------
  // MemoryMapWorker interface
  //------------------------------------
  public:
    void save_entry_index(size_t i) override {
      mmm_index_ = i;
      xassert(MemoryMapManager::get()->check_entry(mmm_index_, this));
    }

    void evict() override {
      mmm_index_ = 0;  // prevent from sending del_entry() signal back
      memunmap();
      xassert(!mapped_ && !mmm_index_);
    }


  //------------------------------------
  // Mapping / Unmapping
  //------------------------------------
  protected:
    virtual void memmap() {
      if (mapped_) return;
      #if DT_OS_WINDOWS
        throw NotImplError() << "Memory-mapping not supported on Windows yet";

      #else
        // Place a mutex lock to prevent multiple threads from trying
        // to perform memory-mapping of different files (or same file)
        // in parallel. If multiple threads called this method at the
        // same time, then only one will proceed, while all others
        // will wait until the lock is released, and then exit because
        // flag `mapped_` will now be true.
        static std::mutex mmp_mutex;
        std::lock_guard<std::mutex> _(mmp_mutex);
        if (mapped_) return;

        bool create = temporary_file_;
        size_t n = size_;

        File file(filename_, create? File::CREATE : File::READ, fd_);
        file.assert_is_not_dir();
        if (create) {
          file.resize(n);
        }
        size_t filesize = file.size();
        if (filesize == 0) {
          // Cannot memory-map 0-bytes file. However we shouldn't
          // really need to: if memory size is 0 then mmp can be NULL
          // as nobody is going to read from it anyways.
          size_ = 0;
          data_ = nullptr;
          mapped_ = true;
          return;
        }
        size_ = filesize + (create? 0 : n);

        // Memory-map the file.
        // In "open" mode if `n` is non-zero, then we will be opening
        // a buffer with larger size than the actual file size. Also,
        // the file is opened in "private, read-write" mode -- meaning
        // that the user can write to that buffer if needed. From the
        // man pages of `mmap`:
        //
        // | MAP_SHARED
        // |   Share this mapping. Updates to the mapping are visible
        // |   to other processes that map this file, and are carried
        // |   through to the underlying file. The file may not
        // |   actually be updated until msync(2) or munmap() is
        // |   called.
        // | MAP_PRIVATE
        // |   Create a private copy-on-write mapping.  Updates to the
        // |   mapping are not carried through to the underlying file.
        // | MAP_NORESERVE
        // |   Do not reserve swap space for this mapping.  When swap
        // |   space is reserved, one has the guarantee that it is
        // |   possible to modify the mapping.  When swap space is not
        // |   reserved one might get SIGSEGV upon a write if no
        // |   physical memory is available.
        //
        int attempts = 3;
        while (attempts--) {
          int flags = create? MAP_SHARED : MAP_PRIVATE|MAP_NORESERVE;
          data_ = mmap(/* address = */ nullptr,
                         /* length = */ size_,
                         /* protection = */ PROT_WRITE|PROT_READ,
                         /* flags = */ flags,
                         /* fd = */ file.descriptor(),
                         /* offset = */ 0);
          if (data_ == MAP_FAILED) {
            data_ = nullptr;
            if (errno == 12) {  // release some memory and try again
              MemoryMapManager::get()->freeup_memory();
              if (attempts) {
                errno = 0;
                continue;
              }
            }
            // Exception is thrown from the constructor -> the base class'
            // destructor will be called, which checks that `data_` is null.
            throw IOError() << "Memory-map failed for file " << file.cname()
                            << " of size " << filesize
                            << " +" << size_ - filesize << Errno;
          } else {
            MemoryMapManager::get()->add_entry(this, size_);
            break;
          }
        }
        mapped_ = true;
        xassert(mmm_index_);
      #endif
    }

    void memunmap() noexcept {
      if (!mapped_) return;
      #if !DT_OS_WINDOWS
        if (data_) {
          int ret = munmap(data_, size_);
          if (ret) {
            // Cannot throw exceptions from a destructor, so just
            // print a message
            printf("Error unmapping the view of file: [errno %d] %s. "
                   "Resources may have not been freed properly.",
                   errno, std::strerror(errno));
          }
          data_ = nullptr;
        }
        mapped_ = false;
        size_ = 0;
        if (mmm_index_) {
          try {
            MemoryMapManager::get()->del_entry(mmm_index_);
          } catch (...) {}
          mmm_index_ = 0;
        }
      #endif
    }
};




//------------------------------------------------------------------------------
// Overmap_BufferImpl
//------------------------------------------------------------------------------

class Overmap_BufferImpl : public Mmap_BufferImpl {
  private:
    void* xbuf_;
    size_t xsize_;

  public:
    Overmap_BufferImpl(const std::string& path, size_t xn, int fd = -1)
      : Mmap_BufferImpl(xn, path, fd, false), xbuf_(nullptr), xsize_(xn)
    {
      writable_ = true;
    }

    ~Overmap_BufferImpl() override {
      if (!xbuf_) return;
      #if !DT_OS_WINDOWS
        int ret = munmap(xbuf_, xsize_);
        if (ret) {
          printf("Cannot unmap extra memory %p: [errno %d] %s",
                 xbuf_, errno, std::strerror(errno));
        }
      #endif
    }

    virtual size_t memory_footprint() const override {
      return Mmap_BufferImpl::memory_footprint() - sizeof(Mmap_BufferImpl) +
             xsize_ + sizeof(Overmap_BufferImpl);
    }

  protected:
    void memmap() override {
      Mmap_BufferImpl::memmap();
      if (xsize_ == 0) return;
      if (!data_) return;
      #if DT_OS_WINDOWS
        throw NotImplError() << "Memory-mapping not supported on Windows yet";

      #else
        // The parent's constructor has opened a memory-mapped region
        // of size `filesize + xn`. This, however, is not always
        // enough:
        // | A file is mapped in multiples of the page size. For a
        // | file that is not a multiple of the page size, the
        // | remaining memory is 0ed when mapped, and writes to that
        // | region are not written out to the file.
        //
        // Thus, when `filesize` is *not* a multiple of pagesize,
        // then the memory mapping will have some writable "scratch"
        // space at the end, filled with '\0' bytes. We check -- if
        // this space is large enough to hold `xn` bytes, then don't
        // do anything extra. If not (for example when `filesize` is
        // an exact multiple of `pagesize`), then attempt to read/
        // write past physical end of file wil fail with a BUS error,
        // despite the fact that the map was overallocated for the
        // extra `xn` bytes:
        // | Use of a mapped region can result in these signals:
        // | SIGBUS:
        // |   Attempted access to a portion of the buffer that does
        // |   not correspond to the file (for example, beyond the
        // |   end of the file)
        //
        // In order to circumvent this, we allocate a new memory-
        // mapped region of size `xn` and placed at address `buf +
        // filesize`. In theory, this should always succeed because
        // we over-allocated `buf` by `xn` bytes; and even though
        // those extra bytes are not readable/writable, at least
        // there is a guarantee that it is not occupied by anyone
        // else. Now, `mmap()` documentation explicitly allows to
        // declare mappings that overlap each other:
        // | MAP_ANONYMOUS:
        // |   The mapping is not backed by any file; its contents are
        // |   initialized to zero. The fd argument is ignored.
        // | MAP_FIXED
        // |   Don't interpret addr as a hint: place the mapping at
        // |   exactly that address.  `addr` must be a multiple of
        // |   the page size. If the memory region specified by addr
        // |   and len overlaps pages of any existing mapping(s), then
        // |   the overlapped part of the existing mapping(s) will be
        // |   discarded.
        //
        size_t xn = xsize_;
        size_t pagesize = static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
        size_t filesize = size() - xn;
        // How much to add to filesize to align it to a page boundary
        size_t gapsize = (pagesize - filesize%pagesize) % pagesize;
        if (xn > gapsize) {
          void* target = static_cast<void*>(
                            static_cast<char*>(data_) + filesize + gapsize);
          xsize_ = xn - gapsize;
          xbuf_ = mmap(/* address = */ target,
                      /* size = */ xsize_,
                      /* protection = */ PROT_WRITE|PROT_READ,
                      /* flags = */ MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED,
                      /* file descriptor, ignored */ -1,
                      /* offset, ignored */ 0);
          if (xbuf_ == MAP_FAILED) {
            throw RuntimeError() << "Cannot allocate additional " << xsize_
                                 << " bytes at address " << target << ": "
                                 << Errno;
          }
        }
      #endif
    }
};





//==============================================================================
// MemoryRange
//==============================================================================

  //---- Constructors ----------------------------

  MemoryRange::MemoryRange(BufferImpl*&& impl)
    : impl_(impl) {}

  MemoryRange::MemoryRange()
    : MemoryRange(new Memory_BufferImpl(0)) {}

  MemoryRange::MemoryRange(const MemoryRange& other)
    : impl_(other.impl_->acquire()) {}

  MemoryRange::MemoryRange(MemoryRange&& other) {
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }

  MemoryRange& MemoryRange::operator=(const MemoryRange& other) {
    auto tmp = impl_;
    impl_ = other.impl_->acquire();
    tmp->release();
    return *this;
  }

  MemoryRange& MemoryRange::operator=(MemoryRange&& other) {
    std::swap(impl_, other.impl_);
    return *this;
  }

  MemoryRange::~MemoryRange() {
    if (impl_) impl_->release();
  }


  MemoryRange MemoryRange::mem(size_t n) {
    return MemoryRange(new Memory_BufferImpl(n));
  }

  MemoryRange MemoryRange::mem(int64_t n) {
    return MemoryRange(new Memory_BufferImpl(static_cast<size_t>(n)));
  }

  MemoryRange MemoryRange::acquire(void* ptr, size_t n) {
    return MemoryRange(new Memory_BufferImpl(n, std::move(ptr)));
  }

  MemoryRange MemoryRange::external(void* ptr, size_t n) {
    return MemoryRange(new External_BufferImpl(n, ptr));
  }

  MemoryRange MemoryRange::external(const void* ptr, size_t n) {
    return MemoryRange(new External_BufferImpl(n, ptr));
  }

  MemoryRange MemoryRange::external(const void* ptr, size_t n, Py_buffer* pb) {
    return MemoryRange(new External_BufferImpl(n, ptr, pb));
  }

  MemoryRange MemoryRange::view(const MemoryRange& src, size_t n, size_t offset) {
    return MemoryRange(new View_BufferImpl(src.impl_, n, offset));
  }

  MemoryRange MemoryRange::mmap(const std::string& path) {
    return MemoryRange(new Mmap_BufferImpl(path));
  }

  MemoryRange MemoryRange::mmap(const std::string& path, size_t n, int fd) {
    return MemoryRange(new Mmap_BufferImpl(n, path, fd));
  }

  MemoryRange MemoryRange::overmap(const std::string& path, size_t extra_n,
                                   int fd)
  {
    return MemoryRange(new Overmap_BufferImpl(path, extra_n, fd));
  }


  //---- Basic properties ------------------------

  MemoryRange::operator bool() const {
    return (impl_->size() != 0);
  }

  bool MemoryRange::is_writable() const {
    return impl_->is_writable();
  }

  bool MemoryRange::is_resizable() const {
    return impl_->is_resizable();
  }

  bool MemoryRange::is_pyobjects() const {
    return impl_->contains_pyobjects_;
  }

  size_t MemoryRange::size() const {
    return impl_->size();
  }

  size_t MemoryRange::memory_footprint() const {
    return sizeof(MemoryRange) + impl_->memory_footprint();
  }


  //---- Main data accessors ---------------------

  const void* MemoryRange::rptr() const {
    return impl_->data();
  }

  const void* MemoryRange::rptr(size_t offset) const {
    const char* ptr0 = static_cast<const char*>(rptr());
    return static_cast<const void*>(ptr0 + offset);
  }

  void* MemoryRange::wptr() {
    if (!is_writable()) materialize();
    return impl_->data();
  }

  void* MemoryRange::wptr(size_t offset) {
    char* ptr0 = static_cast<char*>(wptr());
    return static_cast<void*>(ptr0 + offset);
  }

  void* MemoryRange::xptr() const {
    XAssert(is_writable());
    return impl_->data();
  }

  void* MemoryRange::xptr(size_t offset) const {
    return static_cast<void*>(static_cast<char*>(xptr()) + offset);
  }


  //---- MemoryRange manipulators ----------------

  MemoryRange& MemoryRange::set_pyobjects(bool clear_data) {
    size_t n = impl_->size() / sizeof(PyObject*);
    xassert(n * sizeof(PyObject*) == impl_->size());
    xassert(this->is_writable());
    if (clear_data) {
      PyObject** data = static_cast<PyObject**>(impl_->data());
      for (size_t i = 0; i < n; ++i) {
        data[i] = Py_None;
      }
      Py_None->ob_refcnt += n;
    }
    impl_->contains_pyobjects_ = true;
    return *this;
  }

  MemoryRange& MemoryRange::resize(size_t newsize, bool keep_data) {
    if (!impl_) {
      impl_ = (new Memory_BufferImpl(newsize))->acquire();
    }
    size_t oldsize = impl_->size();
    if (newsize != oldsize) {
      if (is_resizable()) {
        if (impl_->contains_pyobjects_) {
          size_t n_old = oldsize / sizeof(PyObject*);
          size_t n_new = newsize / sizeof(PyObject*);
          if (n_new < n_old) {
            PyObject** data = static_cast<PyObject**>(impl_->data());
            for (size_t i = n_new; i < n_old; ++i) Py_DECREF(data[i]);
          }
          impl_->resize(newsize);
          if (n_new > n_old) {
            PyObject** data = static_cast<PyObject**>(impl_->data());
            for (size_t i = n_old; i < n_new; ++i) data[i] = Py_None;
            Py_None->ob_refcnt += n_new - n_old;
          }
        } else {
          impl_->resize(newsize);
        }
      } else {
        size_t copysize = keep_data? std::min(newsize, oldsize) : 0;
        materialize(newsize, copysize);
      }
    }
    return *this;
  }



  //---- Utility functions -----------------------

  void MemoryRange::verify_integrity() const {
    if (!impl_) {
      throw AssertionError() << "NULL implementation object in MemoryRange";
    }
    impl_->verify_integrity();
  }

  void MemoryRange::materialize() {
    size_t s = impl_->size();
    materialize(s, s);
  }

  void MemoryRange::materialize(size_t newsize, size_t copysize) {
    xassert(newsize >= copysize);
    Memory_BufferImpl* newimpl = new Memory_BufferImpl(newsize);
    if (copysize) {
      std::memcpy(newimpl->data(), impl_->data(), copysize);
    }
    if (impl_->contains_pyobjects_) {
      newimpl->contains_pyobjects_ = true;
      PyObject** newdata = static_cast<PyObject**>(newimpl->data());
      size_t n_new = newsize / sizeof(PyObject*);
      size_t n_copy = copysize / sizeof(PyObject*);
      size_t i = 0;
      for (; i < n_copy; ++i) Py_INCREF(newdata[i]);
      for (; i < n_new; ++i) newdata[i] = Py_None;
      Py_None->ob_refcnt += n_new - n_copy;
    }
    impl_->release();
    impl_ = newimpl->acquire();
  }
