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
#include <fcntl.h>     // open
#include <stdio.h>     // remove
#include <string.h>    // strlen, strerror
#include <sys/mman.h>  // mmap
#include <sys/stat.h>  // fstat
#include <unistd.h>    // access, close, write, lseek, sysconf
#include <algorithm>   // min
#include "datatable_check.h"
#include "myassert.h"
#include "py_utils.h"
#include "utils.h"



//==============================================================================
// Base MemoryBuffer
//==============================================================================

MemoryBuffer::MemoryBuffer()
    : buf(nullptr), allocsize(0), refcount(1), readonly(false) {}

// It is the job of a derived class to clean up the `buf`. Here we merely
// check that the derived class did not forget to do so.
// Note: it is possible to have `refcount > 0` here: this occurs only when the
// destructor is called because the derived class' constructor has thrown an
// exception.
MemoryBuffer::~MemoryBuffer() {
  assert(buf == nullptr);
  // assert(refcount == 0);
}

void* MemoryBuffer::get() const {
  return buf;
}

void* MemoryBuffer::at(size_t n) const {
  return static_cast<void*>(static_cast<char*>(buf) + n);
}

void* MemoryBuffer::at(int64_t n) const {
  return static_cast<void*>(static_cast<char*>(buf) + n);
}

void* MemoryBuffer::at(int32_t n) const {
  return static_cast<void*>(static_cast<char*>(buf) + n);
}

size_t MemoryBuffer::size() const {
  return allocsize;
}

size_t MemoryBuffer::memory_footprint() const {
  return allocsize + sizeof(MemoryBuffer);
}

void MemoryBuffer::resize(UNUSED(size_t n)) {
  throw Error("Resizing this object is not supported");
}

MemoryBuffer* MemoryBuffer::safe_resize(size_t n) {
  if (this->is_readonly()) {
    MemoryBuffer* mb = new MemoryMemBuf(n);
    memcpy(mb->buf, this->buf, std::min(n, this->allocsize));
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
  MemoryMemBuf* res = new MemoryMemBuf(allocsize);
  if (allocsize) {
    memcpy(res->buf, buf, allocsize);
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

MemoryMemBuf::MemoryMemBuf(size_t n) {
  if (n) {
    allocsize = n;
    buf = malloc(n);
    if (buf == nullptr) throw Error("Unable to allocate memory of size %zu", n);
  }
}

MemoryMemBuf::MemoryMemBuf(void *ptr, size_t n) {
  if (n) {
    allocsize = n;
    buf = ptr;
    if (buf == nullptr) throw Error("Unallocated memory region provided");
  }
}

MemoryMemBuf::~MemoryMemBuf() {
  free(buf);
  buf = nullptr;
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
    void *ptr = realloc(buf, n);
    if (!ptr) throw Error("Unable to reallocate memory to size %zu", n);
    buf = ptr;
  } else if (buf) {
    free(buf);
    buf = nullptr;
  }
  allocsize = n;
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
  buf = nullptr;
  if (pybufinfo) {
    PyBuffer_Release(static_cast<Py_buffer*>(pybufinfo));
  }
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
// Disk-based MemoryBuffer
//==============================================================================


MemmapMemBuf::MemmapMemBuf(const std::string& path, size_t n, bool create)
    : filename(path), xbuf(nullptr), xbuf_size(0)
{
  if (create) {
    FILE *fp = fopen(filename.c_str(), "w");
    if (!fp) throw Error("Cannot open file");
    if (n) {
      fseek(fp, (long)(n - 1), SEEK_SET);
      fputc('\0', fp);
    }
    fclose(fp);
    readonly = false;
  }
  else {
    if (access(filename.c_str(), R_OK) == -1) {
      throw Error("File %s cannot be accessed: %s",
                  filename.c_str(), strerror(errno));
    }
    readonly = true;
  }

  // Open the file and determine its size
  int fd = open(filename.c_str(), create? O_RDWR : O_RDONLY);
  if (fd == -1) {
    throw Error("Cannot open file %s: %s",
                filename.c_str(), strerror(errno));
  }
  struct stat statbuf;
  int ret = fstat(fd, &statbuf);
  if (ret == -1) {
    throw Error("Error in fstat(): %s", strerror(errno));
  }
  if (S_ISDIR(statbuf.st_mode)) {
    close(fd);
    throw Error("File %s is a directory", filename.c_str());
  }
  size_t filesize = static_cast<size_t>(statbuf.st_size);
  allocsize = filesize + (create? 0 : n);

  // Memory-map the file.
  // In "open" mode if `n` is non-zero, then we will be opening a buffer
  // with larger size than the actual file size. Also, the file is opened in
  // "private, read-write" mode -- meaning that the user can write to that
  // buffer if needed.
  // From the man pages of `mmap`:
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
             /* file descriptor = */ fd,
             /* offset = */ 0);
  close(fd);  // fd is no longer needed
  if (buf == MAP_FAILED) {
    // Exception is thrown from the constructor -> the base class' destructor
    // will be called, which checks that `buf` is null.
    buf = nullptr;
    throw Error("Memory-map failed for file %s of size %zu: [%d] %s",
                filename.c_str(), filesize, errno, strerror(errno));
  }

  // Determine if additional memory-mapped region is necessary (only when
  // opening an existing file, and `n > 0` bytes are requested).
  // From `mmap`'s man page:
  // | A file is mapped in multiples of the page size. For a file that is
  // | not a multiple of the page size, the remaining memory is 0ed when
  // | mapped, and writes to that region are not written out to the file.
  //
  // Thus, when `filesize` is *not* a multiple of pagesize, then the
  // memory mapping will have some writable "scratch" space at the end,
  // filled with '\0' bytes. We check -- if this space is large enough to
  // hold `n` bytes, then don't do anything extra. If not (for example
  // when `filesize` is an exact multiple of `pagesize`), then attempt to
  // read/write past physical end of file wil fail with a BUS error -- despite
  // the fact that the map was overallocated for the extra `n` bytes:
  // | Use of a mapped region can result in these signals:
  // | SIGBUS:
  // |   Attempted access to a portion of the buffer that does not
  // |   correspond to the file (for example, beyond the end of the file)
  //
  // In order to circumvent this, we allocate a new memory-mapped region of size
  // `n` and placed at address `buf + filesize`. In theory, this should always
  // succeed because we over-allocated `buf` by `n` bytes; and even though
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
  // How much to add to filesize to align it to a page boundary
  size_t gapsize = (pagesize - filesize%pagesize) % pagesize;
  if (!create && n > gapsize) {
    void* target = this->at(filesize + gapsize);
    xbuf_size = n - gapsize;
    xbuf = mmap(/* address = */ target,
                /* size = */ xbuf_size,
                /* protection = */ PROT_WRITE|PROT_READ,
                /* flags - */ MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED,
                /* file descriptor, ignored */ -1,
                /* offset = */ 0);
    if (xbuf == MAP_FAILED) {
      munmap(buf, allocsize);
      buf = nullptr;
      xbuf = nullptr;
      throw Error("Cannot allocate additional %zu bytes at address %p",
                  xbuf_size, target);
    }
  }
}


MemmapMemBuf::~MemmapMemBuf() {
  int ret = munmap(buf, allocsize);
  if (ret) {
    // Too bad we can't throw exceptions from a destructor...
    printf("System error '%s' unmapping the view of file. Resources may have "
           "been not freed properly.", strerror(errno));
  }
  buf = nullptr;  // Checked in the upstream destructor
  if (xbuf) {
    ret = munmap(xbuf, xbuf_size);
    if (ret) printf("Cannot unmap extra memory %p: %s", xbuf, strerror(errno));
    xbuf = nullptr;
  }
  if (!is_readonly()) {
    remove(filename.c_str());
  }
}


void MemmapMemBuf::resize(size_t n) {
  if (is_readonly()) throw Error("Cannot resize a readonly buffer");
  munmap(buf, allocsize);
  buf = nullptr;
  int ret = truncate(filename.c_str(), (off_t)n);
  if (ret == -1) {
    throw Error("Cannot truncate file %s to size %zu: [errno %d] %s",
                filename.c_str(), n, errno, strerror(errno));
  }

  int fd = open(filename.c_str(), O_RDWR);
  if (fd == -1) {
    THROW_ERROR("Unable to open file %s: %s", filename.c_str(), strerror(errno));
  }
  buf = mmap(NULL, n, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
  allocsize = n;
  close(fd);
  if (buf == MAP_FAILED) {
    buf = nullptr;
    THROW_ERROR("Memory map failed: %s", strerror(errno));
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
