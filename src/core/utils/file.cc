//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <errno.h>      // errno
#include <fcntl.h>      // open, posix_fallocate[?]
#include <stdio.h>      // remove
#include <string.h>     // strerror
#include <sys/stat.h>   // fstat
#include "utils/file.h"
#include "utils/macros.h"
#include "utils/misc.h"

#if DT_OS_WINDOWS
  #include <io.h>               // close, write, _chsize
  #define FTRUNCATE _chsize
#else
  #include <unistd.h>           // close, write, ftruncate
  #define FTRUNCATE ftruncate
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
    fd = fileno;
    flags = File::EXTERNALFD;
  } else {
    fd = open(file.c_str(), oflags, mode);
    flags = oflags;
    if (fd == -1) {
      throw IOError() << "Cannot open file " << file << ": " << Errno;
    }
  }
  name = file;
  // size of -1 is used to indicate that `statbuf` was not loaded yet
  statbuf.st_size = -1;
}

File::~File() {
  if (fd != -1 && flags != File::EXTERNALFD) {
    int ret = close(fd);
    if (ret == -1) {
      // Cannot throw an exception from a destructor, so just print a message
      printf("Error closing file %s (fd = %d): [errno %d] %s",
             name.c_str(), fd, errno, strerror(errno));
    }
  }
}


//------------------------------------------------------------------------------

int File::descriptor() const {
  return fd;
}

size_t File::size() const {
  load_stats();
  return static_cast<size_t>(statbuf.st_size);
}

// Same as `size()`, but static (i.e. no need to open the file).
size_t File::asize(const std::string& name) {
  struct stat statbuf;
  int ret = stat(name.c_str(), &statbuf);
  if (ret == -1) {
    throw IOError() << "Unable to obtain size of " << name << ": " << Errno;
  }
  return static_cast<size_t>(statbuf.st_size);
}

const char* File::cname() const {
  return name.c_str();
}


void File::resize(size_t newsize) {
  int ret = FTRUNCATE(fd, static_cast<off_t>(newsize));
  if (ret == -1) {
    throw IOError() << "Unable to truncate() file " << name
                    << " to size " << newsize << ": " << Errno;
  }
  statbuf.st_size = -1;  // force reload stats on next request

  off_t sz = static_cast<off_t>(newsize);
  if (!sz) return;
  #ifdef HAVE_POSIX_FALLOCATE
    // The function posix_fallocate() ensures that disk space is allocated
    // for the file referred to by the descriptor fd for the bytes in the
    // range starting at offset and continuing for len bytes. After a successful
    // call to posix_fallocate(), subsequent writes to bytes in the specified
    // range are guaranteed not to fail because of lack of disk space.
    // (https://linux.die.net/man/3/posix_fallocate)
    //
    ret = posix_fallocate(fd, 0, sz);
    if (ret == ENOSPC) {
      throw IOError() << "Unable to create file " << name << " of size "
          << newsize << ": not enough space left on device";
    }
    if (ret) {
      throw IOError() << "Unable to fallocate() file " << name
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
    ret = fcntl(fd, F_PREALLOCATE, &store);
    if (ret == -1) {
      store.fst_flags = F_ALLOCATEALL;
      ret = fcntl(fd, F_PREALLOCATE, &store);
      if (ret == -1 && errno != EINVAL) {
        throw IOError() << "Unable to create file " << name << " of size "
            << newsize << ": " << Errno;
      }
    }
  #endif
}


void File::assert_is_not_dir() const {
  load_stats();
  if (S_ISDIR(statbuf.st_mode)) {
    throw IOError() << "File " << name << " is a directory";
  }
}

void File::load_stats() const {
  if (statbuf.st_size >= 0) return;
  int ret = fstat(fd, &statbuf);
  if (ret == -1) {
    throw IOError() << "Error in fstat() for file " << name << ": " << Errno;
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
