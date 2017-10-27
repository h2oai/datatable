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
#include "memorybuf.h"
#include <errno.h>     // errno
#include <string.h>    // strlen, strerror
#include <sys/mman.h>  // mmap
#include <unistd.h>    // sysconf
#include <algorithm>   // min
#include "datatable_check.h"
#include "file.h"
#include "myassert.h"
#include "py_utils.h"
#include "utils.h"



//==============================================================================
// Base MemoryBuffer
//==============================================================================

MemoryBuffer::MemoryBuffer() : refcount(1), readonly(false) {}

// Note: it is possible to have `refcount > 0` here: this occurs only when the
// destructor is called because the derived class' constructor has thrown an
// exception.
MemoryBuffer::~MemoryBuffer() {}


void* MemoryBuffer::at(size_t n) const {
  return static_cast<void*>(static_cast<char*>(get()) + n);
}

void* MemoryBuffer::at(int64_t n) const {
  return static_cast<void*>(static_cast<char*>(get()) + n);
}

void* MemoryBuffer::at(int32_t n) const {
  return static_cast<void*>(static_cast<char*>(get()) + n);
}

void MemoryBuffer::resize(size_t) {
  throw Error("Resizing this object is not supported");
}

MemoryBuffer* MemoryBuffer::safe_resize(size_t n) {
  if (this->is_readonly()) {
    MemoryBuffer* mb = new MemoryMemBuf(n);
    memcpy(mb->get(), this->get(), std::min(n, this->size()));
    // Note: this may delete the current object! Do not access `this` after
    // this call: it may have become a dangling pointer.
    this->release();
    return mb;
  } else {
    this->resize(n);
    return this;
  }
}

bool MemoryBuffer::is_readonly() const {
  return readonly || refcount > 1;
}

PyObject* MemoryBuffer::pyrepr() const {
  return none();
}

MemoryBuffer* MemoryBuffer::shallowcopy() {
  ++refcount;
  return this;
}

MemoryMemBuf* MemoryBuffer::deepcopy() const {
  size_t allocsize = size();
  MemoryMemBuf* res = new MemoryMemBuf(allocsize);
  if (allocsize) {
    memcpy(res->get(), this->get(), allocsize);
  }
  return res;
}

void MemoryBuffer::release() {
  --refcount;
  if (refcount == 0) {
    delete this;
  }
}

int MemoryBuffer::get_refcount() const {
  return refcount;
}

bool MemoryBuffer::verify_integrity(IntegrityCheckContext& icc,
                                    const std::string& name) const
{
  if (refcount <= 0) {
    icc << name << "'s refcount is non-positive: " << refcount << icc.end();
    return false;
  }
  return true;
}



//==============================================================================
// Memory-based MemoryBuffer
//==============================================================================

MemoryMemBuf::MemoryMemBuf() : buf(nullptr), allocsize(0) {}

MemoryMemBuf::MemoryMemBuf(size_t n) : MemoryMemBuf() {
  if (n) {
    allocsize = n;
    buf = malloc(n);
    if (buf == nullptr) throw Error("Unable to allocate memory of size %zu", n);
  }
}

MemoryMemBuf::MemoryMemBuf(void* ptr, size_t n) : MemoryMemBuf() {
  if (n) {
    allocsize = n;
    buf = ptr;
    if (buf == nullptr) throw Error("Unallocated memory region provided");
  }
}

MemoryMemBuf::~MemoryMemBuf() {
  free(buf);
}

void* MemoryMemBuf::get() const {
  return buf;
}

size_t MemoryMemBuf::size() const {
  return allocsize;
}

void MemoryMemBuf::resize(size_t n) {
  // The documentation for `void* realloc(void* ptr, size_t new_size);` says
  // the following:
  // | If there is not enough memory, the old memory block is not freed and
  // | null pointer is returned.
  // | If new_size is zero, the behavior is implementation defined (null
  // | pointer may be returned (in which case the old memory block may or may
  // | not be freed), or some non-null pointer may be returned that may not be
  // | used to access storage). Support for zero size is deprecated as of
  // | C11 DR 400.
  if (n == allocsize) return;
  if (n) {
    void* ptr = realloc(buf, n);
    if (!ptr) throw Error("Unable to reallocate memory to size %zu", n);
    buf = ptr;
  } else if (buf) {
    free(buf);
    buf = nullptr;
  }
  allocsize = n;
}


size_t MemoryMemBuf::memory_footprint() const {
  return sizeof(MemoryMemBuf) + allocsize;
}


PyObject* MemoryMemBuf::pyrepr() const {
  static PyObject* r = PyUnicode_FromString("data");
  return incref(r);
}


bool MemoryMemBuf::verify_integrity(IntegrityCheckContext& icc,
                                    const std::string& name) const
{
  int nerrs = icc.n_errors();
  auto end = icc.end();

  MemoryBuffer::verify_integrity(icc, name);

  if (buf && allocsize) {
    size_t actual_allocsize = malloc_size(buf);
    if (allocsize > actual_allocsize) {
      icc << name << " has allocsize=" << allocsize << ", while the internal "
          << "buffer is allocated for " << actual_allocsize << " bytes only"
          << end;
    }
  }
  // else if (buf && !allocsize) {
  //   icc << name << " has the internal buffer allocated (" << buf
  //       << "), while allocsize is 0" << end;
  // }
  else if (!buf && allocsize) {
    icc << name << " has the internal memory buffer not allocated, whereas "
        << "its allocsize is " << allocsize << end;
  }

  return !icc.has_errors(nerrs);
}



//==============================================================================
// External MemoryBuffer
//==============================================================================

ExternalMemBuf::ExternalMemBuf(void* ptr, void* pybuf, size_t size) {
  buf = ptr;
  allocsize = size;
  pybufinfo = pybuf;
  readonly = true;
  if (buf == nullptr && allocsize > 0) {
    throw Error("Unallocated buffer supplied to the ExternalMemBuf() "
                "constructor, exptected memory region of size %zu", size);
  }
}

ExternalMemBuf::ExternalMemBuf(void* ptr, size_t n)
    : ExternalMemBuf(ptr, nullptr, n) {}

ExternalMemBuf::ExternalMemBuf(const char* str)
    : ExternalMemBuf(const_cast<char*>(str),
                     nullptr,
                     strlen(str) + 1) {}


ExternalMemBuf::~ExternalMemBuf() {
  if (pybufinfo) {
    PyBuffer_Release(static_cast<Py_buffer*>(pybufinfo));
  }
}

void* ExternalMemBuf::get() const {
  return buf;
}

size_t ExternalMemBuf::size() const {
  return allocsize;
}

size_t ExternalMemBuf::memory_footprint() const {
  size_t sz = allocsize + sizeof(ExternalMemBuf);
  if (pybufinfo) sz += sizeof(Py_buffer);
  return sz;
}

PyObject* ExternalMemBuf::pyrepr() const {
  static PyObject* r = PyUnicode_FromString("xbuf");
  return incref(r);
}

bool ExternalMemBuf::verify_integrity(IntegrityCheckContext& icc,
                                      const std::string& name) const
{
  int nerrs = icc.n_errors();
  MemoryBuffer::verify_integrity(icc, name);
  // Not much we can do about checking the validity of `buf`, unfortunately. It
  // is provided by an external source, and could in theory point to anything...
  if (allocsize && !buf) {
    icc << "Internal data pointer in " << name << " is null" << icc.end();
  }
  return !icc.has_errors(nerrs);
}



//==============================================================================
// MemoryBuffer based on a memmapped file
//==============================================================================

MemmapMemBuf::MemmapMemBuf(const std::string& path)
    : MemmapMemBuf(path, 0, false) {}


MemmapMemBuf::MemmapMemBuf(const std::string& path, size_t n)
    : MemmapMemBuf(path, n, true) {}


MemmapMemBuf::MemmapMemBuf(const std::string& path, size_t n, bool create)
    : buf(nullptr), allocsize(0), filename(path)
{
  readonly = !create;

  File file(filename, create? File::CREATE : File::READ);
  file.assert_is_not_dir();
  if (create) {
    file.resize(n);
  }
  size_t filesize = file.size();
  allocsize = filesize + (create? 0 : n);

  // Memory-map the file.
  // In "open" mode if `n` is non-zero, then we will be opening a buffer
  // with larger size than the actual file size. Also, the file is opened in
  // "private, read-write" mode -- meaning that the user can write to that
  // buffer if needed. From the man pages of `mmap`:
  //
  // | MAP_SHARED
  // |   Share this mapping. Updates to the mapping are visible to other
  // |   processes that map this file, and are carried through to the underlying
  // |   file. The file may not actually be updated until msync(2) or munmap()
  // |   is called.
  // | MAP_PRIVATE
  // |   Create a private copy-on-write mapping.  Updates to the mapping
  // |   are not carried through to the underlying file.
  // | MAP_NORESERVE
  // |   Do not reserve swap space for this mapping.  When swap space is
  // |   reserved, one has the guarantee that it is possible to modify the
  // |   mapping.  When swap space is not reserved one might get SIGSEGV
  // |   upon a write if no physical memory is available.
  //
  buf = mmap(/* address = */ NULL,
             /* length = */ allocsize,
             /* protection = */ PROT_WRITE|PROT_READ,
             /* flags = */ create? MAP_SHARED : MAP_PRIVATE|MAP_NORESERVE,
             /* fd = */ file.descriptor(),
             /* offset = */ 0);
  if (buf == MAP_FAILED) {
    // Exception is thrown from the constructor -> the base class' destructor
    // will be called, which checks that `buf` is null.
    buf = nullptr;
    throw Error("Memory-map failed for file %s of size %zu+%zu: [%d] %s",
                file.cname(), filesize, allocsize - filesize,
                errno, strerror(errno));
  }
}


MemmapMemBuf::~MemmapMemBuf() {
  int ret = munmap(buf, allocsize);
  if (ret) {
    // Cannot throw exceptions from a destructor, so just print a message
    printf("Error unmapping the view of file: [errno %d] %s. Resources may "
           "have not been freed properly.", errno, strerror(errno));
  }
  buf = nullptr;  // Checked in the upstream destructor
  if (!is_readonly()) {
    File::remove(filename);
  }
}

void* MemmapMemBuf::get() const {
  return buf;
}

size_t MemmapMemBuf::size() const {
  return allocsize;
}


void MemmapMemBuf::resize(size_t n) {
  if (is_readonly()) throw Error("Cannot resize a readonly buffer");
  munmap(buf, allocsize);
  buf = nullptr;

  File file(filename, File::READWRITE);
  file.resize(n);
  buf = mmap(NULL, n, PROT_WRITE|PROT_READ, MAP_SHARED, file.descriptor(), 0);
  allocsize = n;
  if (buf == MAP_FAILED) {
    buf = nullptr;
    throw Error("Memory map failed for file %s when resizing to %zu: "
                "[errno %d] %s", file.cname(), n, errno, strerror(errno));
  }
}


size_t MemmapMemBuf::memory_footprint() const {
  return allocsize + filename.size() + sizeof(MemmapMemBuf);
}


PyObject* MemmapMemBuf::pyrepr() const {
  static PyObject* r = PyUnicode_FromString("mmap");
  return incref(r);
}


bool MemmapMemBuf::verify_integrity(IntegrityCheckContext& icc,
                                    const std::string& name) const
{
  int nerrs = icc.n_errors();
  MemoryBuffer::verify_integrity(icc, name);
  if (!buf) {
    icc << "Memory-map pointer in " << name << " is null" << icc.end();
  }
  return !icc.has_errors(nerrs);
}



//==============================================================================
// MemoryBuffer based on an "overmapped" memmapped file
//==============================================================================

OvermapMemBuf::OvermapMemBuf(const std::string& path, size_t xn)
    : MemmapMemBuf(path, xn, false)
{
  xbuf = nullptr;
  xbuf_size = 0;
  if (xn == 0) return;

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
  // read/write past physical end of file wil fail with a BUS error -- despite
  // the fact that the map was overallocated for the extra `xn` bytes:
  // | Use of a mapped region can result in these signals:
  // | SIGBUS:
  // |   Attempted access to a portion of the buffer that does not
  // |   correspond to the file (for example, beyond the end of the file)
  //
  // In order to circumvent this, we allocate a new memory-mapped region of size
  // `xn` and placed at address `buf + filesize`. In theory, this should always
  // succeed because we over-allocated `buf` by `xn` bytes; and even though
  // those extra bytes are not readable/writable, at least there is a guarantee
  // that it is not occupied by anyone else. Now, `mmap()` documentation
  // explicitly allows to declare mappings that overlap each other:
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
  size_t pagesize = static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
  size_t filesize = size() - xn;
  // How much to add to filesize to align it to a page boundary
  size_t gapsize = (pagesize - filesize%pagesize) % pagesize;
  if (xn > gapsize) {
    void* target = this->at(filesize + gapsize);
    xbuf_size = xn - gapsize;
    xbuf = mmap(/* address = */ target,
                /* size = */ xbuf_size,
                /* protection = */ PROT_WRITE|PROT_READ,
                /* flags = */ MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED,
                /* file descriptor, ignored */ -1,
                /* offset, ignored */ 0);
    if (xbuf == MAP_FAILED) {
      throw Error("Cannot allocate additional %zu bytes at address %p: "
                  "[errno %d] %s", xbuf_size, target, errno, strerror(errno));
    }
  }
}


OvermapMemBuf::~OvermapMemBuf() {
  if (!xbuf) return;
  int ret = munmap(xbuf, xbuf_size);
  if (ret) {
    printf("Cannot unmap extra memory %p: [errno %d] %s",
           xbuf, errno, strerror(errno));
  }
}


void OvermapMemBuf::resize(size_t) {
  throw Error("Objects of class OvermapMemBuf cannot be resized");
}


size_t OvermapMemBuf::memory_footprint() const {
  return (MemmapMemBuf::memory_footprint() - sizeof(MemmapMemBuf) +
          xbuf_size + sizeof(OvermapMemBuf));
}


PyObject* OvermapMemBuf::pyrepr() const {
  static PyObject* r = PyUnicode_FromString("omap");
  return incref(r);
}

bool OvermapMemBuf::verify_integrity(
    IntegrityCheckContext& icc, const std::string& name) const
{
  int nerrs = icc.n_errors();
  MemmapMemBuf::verify_integrity(icc, name);
  if (xbuf_size && !xbuf) {
    icc << name << " has xbuf_size=" << xbuf_size << ", but its xbuf is null"
        << icc.end();
  }
  if (xbuf && !xbuf_size) {
    icc << name << " has xbuf=" << xbuf << ", but its xbuf_size is 0"
        << icc.end();
  }
  return !icc.has_errors(nerrs);
}
