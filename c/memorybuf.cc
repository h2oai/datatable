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
#include <string.h>    // strlen
#include <sys/mman.h>  // mmap
#include <sys/stat.h>  // fstat
#include <unistd.h>    // access, close, write, lseek
#include "py_utils.h"
#include "utils.h"



//==============================================================================
// Base MemoryBuffer
//==============================================================================

MemoryBuffer::MemoryBuffer()
    : buf(nullptr), allocsize(0), readonly(false) {}

MemoryBuffer::~MemoryBuffer() {}

void MemoryBuffer::replace_buffer(void* ptr, size_t sz) {
  buf = ptr;
  allocsize = sz;
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

bool MemoryBuffer::is_readonly() const {
  return readonly;
}

void MemoryBuffer::set_readonly(bool on) {
  readonly = on;
}

PyObject* MemoryBuffer::pyrepr() const {
  return none();
}



//==============================================================================
// Memory-based MemoryBuffer
//==============================================================================

MemoryMemBuf::MemoryMemBuf(size_t n) {
  void* ptr = malloc(n);
  if (!ptr) throw Error("Unable to allocate memory of size %zu", n);
  replace_buffer(ptr, n);
}

MemoryMemBuf::MemoryMemBuf(void *ptr, size_t n) {
  if (!ptr && n) throw Error("Unallocated memory region provided");
  replace_buffer(ptr, n);
}

MemoryMemBuf::~MemoryMemBuf() {
  void *buf = get();
  free(buf);
}

void MemoryMemBuf::resize(size_t n) {
  if (n == size()) return;
  void *ptr = realloc(get(), n);
  if (!ptr) throw Error("Unable to allocate memory of size %zu", n);
  replace_buffer(ptr, n);
}

PyObject* MemoryMemBuf::pyrepr() const {
  static PyObject* r = PyUnicode_FromString("data");
  return incref(r);
}



//==============================================================================
// String MemoryBuffer
//==============================================================================

StringMemBuf::StringMemBuf(const char *str) {
  replace_buffer(static_cast<void*>(const_cast<char*>(str)), strlen(str));
  set_readonly();
}

PyObject* StringMemBuf::pyrepr() const {
  static PyObject* r = PyUnicode_FromString("string");
  return incref(r);
}



//==============================================================================
// External MemoryBuffer
//==============================================================================

ExternalMemBuf::ExternalMemBuf(void *ptr, void *pybuf, size_t size) {
  replace_buffer(ptr, size);
  pybufinfo = pybuf;
  set_readonly();
}

ExternalMemBuf::~ExternalMemBuf() {
  PyBuffer_Release(static_cast<Py_buffer*>(pybufinfo));
}

size_t ExternalMemBuf::memory_footprint() const {
  return size() + sizeof(ExternalMemBuf) + sizeof(Py_buffer);
}

PyObject* ExternalMemBuf::pyrepr() const {
  static PyObject* r = PyUnicode_FromString("xbuf");
  return incref(r);
}



//==============================================================================
// Disk-based MemoryBuffer
//==============================================================================

MemmapMemBuf::MemmapMemBuf(const char *path, size_t n, bool create)
    : filename(path)
{
  if (create) {
    FILE *fp = fopen(filename.c_str(), "w");
    if (!fp) throw Error("Cannot open file");
    if (n) {
      fseek(fp, (long)(n - 1), SEEK_SET);
      fputc('\0', fp);
    }
    fclose(fp);
  } else {
    if (access(filename.c_str(), R_OK) == -1) {
      throw Error("File cannot be opened");
    }
  }

  // Open the file and determine its size
  int fd = open(filename.c_str(), create? O_RDWR : O_RDONLY, 0666);
  if (fd == -1) throw Error("Cannot open file");
  struct stat statbuf;
  if (fstat(fd, &statbuf) == -1) throw Error("Error in fstat()");
  if (S_ISDIR(statbuf.st_mode)) throw Error("File is a directory");
  n = (size_t) statbuf.st_size;

  // Memory-map the file.
  int flags = create? MAP_SHARED : MAP_PRIVATE|MAP_NORESERVE;
  void* buf = mmap(/* addr = */ NULL, /* length = */ n,
                   /* protection = */ PROT_WRITE|PROT_READ,
                   flags, fd, /* offset = */ 0);
  close(fd);  // fd is no longer needed
  if (buf == MAP_FAILED) {
    THROW_ERROR("Memory map failed: %s", strerror(errno));
  }
  replace_buffer(buf, n);
  set_readonly(!create);
}


MemmapMemBuf::~MemmapMemBuf() {
  munmap(get(), size());
  if (!is_readonly()) {
    remove(filename.c_str());
  }
}


void MemmapMemBuf::resize(size_t n) {
  if (is_readonly()) return;
  munmap(get(), size());
  truncate(filename.c_str(), (off_t)n);

  int fd = open(filename.c_str(), O_RDWR);
  if (fd == -1) {
    THROW_ERROR("Unable to open file %s: %s", filename.c_str(), strerror(errno));
  }
  void* buf = mmap(NULL, n, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (buf == MAP_FAILED) {
    THROW_ERROR("Memory map failed: %s", strerror(errno));
  }
  replace_buffer(buf, n);
}


size_t MemmapMemBuf::memory_footprint() const {
  return size() + filename.size() + sizeof(MemmapMemBuf);
}


PyObject* MemmapMemBuf::pyrepr() const {
  static PyObject* r = PyUnicode_FromString("mmap");
  return incref(r);
}
