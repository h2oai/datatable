//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include <errno.h>      // errno
#include <fcntl.h>      // open, posix_fallocate[?]
#include <stdio.h>      // remove
#include <string.h>     // strerror
#include <sys/stat.h>   // fstat
#include "utils/file.h"
#include "utils/misc.h"

#if DT_OS_WINDOWS
  #include <Windows.h>
  #include <io.h>               // close, write, _chsize
  #define FTRUNCATE _chsize_s
  #define FSTAT _fstat64
#else
  #include <unistd.h>           // access, close, write, ftruncate
  #define FTRUNCATE ftruncate
  #define FSTAT fstat
#endif

const int File::READ = O_RDONLY;
const int File::READWRITE = O_RDWR;
const int File::CREATE = O_RDWR | O_CREAT;
const int File::OVERWRITE = O_RDWR | O_CREAT | O_TRUNC;
const int File::APPEND = O_WRONLY | O_CREAT | O_APPEND;
const int File::EXTERNALFD = -1;


//------------------------------------------------------------------------------

File::File(const std::string& file)
    : File(file, READ, 0) {}

File::File(const std::string& file, int oflags, int fileno, mode_t mode) {
  if (fileno > 0) {
    fd_ = fileno;
    flags_ = File::EXTERNALFD;
  } else {
    fd_ = open(file.c_str(), oflags, mode);
    flags_ = oflags;
    if (fd_ == -1) {
      throw IOError() << "Cannot open file " << file << ": " << Errno;
    }
  }
  name_ = file;
  // size of -1 is used to indicate that `statbuf_` was not loaded yet
  statbuf_.st_size = -1;
}

File::~File() {
  if (fd_ != -1 && flags_ != File::EXTERNALFD) {
    int ret = close(fd_);
    if (ret == -1) {
      // Cannot throw an exception from a destructor, so just print a message
      printf("Error closing file %s (fd_ = %d): [errno %d] %s",
             name_.c_str(), fd_, errno, strerror(errno));
    }
  }
}


//------------------------------------------------------------------------------

int File::descriptor() const {
  return fd_;
}

size_t File::size() const {
  load_stats();
  return static_cast<size_t>(statbuf_.st_size);
}

// Same as `size()`, but static (i.e. no need to open the file).
size_t File::asize(const std::string& name) {
  struct STAT statbuf;
  int ret = STAT(name.c_str(), &statbuf);
  if (ret == -1) {
    throw IOError() << "Unable to obtain size of " << name << ": " << Errno;
  }
  return static_cast<size_t>(statbuf.st_size);
}

const char* File::cname() const {
  return name_.c_str();
}


void File::resize(size_t newsize) {
  int ret = FTRUNCATE(fd_, static_cast<int64_t>(newsize)) ;
  if (ret == -1) {
    throw IOError() << "Unable to truncate() file " << name_
                    << " to size " << newsize << ": " << Errno;
  }
  statbuf_.st_size = -1;  // force reload stats on next request

  off_t sz = static_cast<off_t>(newsize);
  if (!sz) return;
  #ifdef HAVE_POSIX_FALLOCATE
    // The function posix_fallocate() ensures that disk space is allocated
    // for the file referred to by the descriptor fd_ for the bytes in the
    // range starting at offset and continuing for len bytes. After a successful
    // call to posix_fallocate(), subsequent writes to bytes in the specified
    // range are guaranteed not to fail because of lack of disk space.
    // (https://linux.die.net/man/3/posix_fallocate)
    //
    ret = posix_fallocate(fd_, 0, sz);
    if (ret == ENOSPC) {
      throw IOError() << "Unable to create file " << name_ << " of size "
          << newsize << ": not enough space left on device";
    }
    if (ret) {
      throw IOError() << "Unable to fallocate() file " << name_
          << ": error " << ret;
    }
  #elif defined __APPLE__
    // On MacOS, use `fncl()` with `cmd=F_PREALLOCATE`. There are 2 modes of
    // allocation: F_ALLOCATECONTIG and F_ALLOCATEALL. We will try both in
    // turn, hoping at least one succeeds.
    // See https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fcntl.2.html
    //
    // On MacOS pre 10.14.4 `fnctl()` sometimes returned EINVAL when called
    // shortly after the file was created...
    //
    fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, sz, 0};
    ret = fcntl(fd_, F_PREALLOCATE, &store);
    if (ret == -1) {
      store.fst_flags = F_ALLOCATEALL;
      ret = fcntl(fd_, F_PREALLOCATE, &store);
      if (ret == -1 && errno != EINVAL) {
        throw IOError() << "Unable to create file " << name_ << " of size "
            << newsize << ": " << Errno;
      }
    }
  #endif
}


void File::assert_is_not_dir() const {
  load_stats();
  #if DT_OS_WINDOWS
    bool isdir = (statbuf_.st_mode & S_IFMT) == S_IFDIR;
  #else
    bool isdir = S_ISDIR(statbuf_.st_mode);
  #endif
  if (isdir) {
    throw IOError() << "File " << name_ << " is a directory";
  }
}


void File::load_stats() const {
  if (statbuf_.st_size >= 0) return;
  int ret = FSTAT(fd_, &statbuf_);
  if (ret == -1) {
    throw IOError() << "Error in fstat() for file " << name_ << ": " << Errno;
  }
}


void File::remove(const std::string& name, bool except) {
  int ret = ::remove(name.c_str());
  if (ret == -1) {
    if (except) {
      throw IOError() << "Unable to remove file " << name << ": " << Errno;
    } else {
      printf("Unable to remove file %s: [errno %d] %s",
             name.c_str(), errno, strerror(errno));
    }
  }
}


bool File::exists(const std::string& name) noexcept {
  #if DT_OS_WINDOWS
    DWORD attrs = GetFileAttributes(name.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) &&
            !(attrs & FILE_ATTRIBUTE_DIRECTORY);

  #else
    int ret = ::access(name.c_str(), F_OK);
    return (ret == 0);
  #endif
}


/**
  * Return true if the file exists and is non-empty, and false
  * otherwise.
  */
bool File::nonempty(const std::string& name) noexcept {
  #if DT_OS_WINDOWS
    HANDLE hFile = CreateFile(
        name.c_str(),           // lpFileName
        FILE_READ_ATTRIBUTES,   // dwDesiredAccess
        FILE_SHARE_WRITE,       // dwShareMode
        nullptr,                // lpSecurityAttributes
        OPEN_EXISTING,          // dwCreationDisposition
        0,                      // dwFlagsAndAttributes
        nullptr                 // hTemplateFile
    );
    if (hFile == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    bool ret = GetFileSizeEx(hFile, &size);
    return ret && (size.QuadPart != 0);

  #else
    struct stat statbuf;
    int ret = stat(name.c_str(), &statbuf);
    return (ret == 0) && S_ISREG(statbuf.st_mode) && (statbuf.st_size > 0);
  #endif
}
