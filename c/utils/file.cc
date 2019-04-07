//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <errno.h>      // errno
#include <fcntl.h>      // open
#include <stdio.h>      // remove
#include <string.h>     // strerror
#include <sys/stat.h>   // fstat
#include <unistd.h>     // close, write, ftruncate
#include "utils/file.h"
#include "utils/misc.h"


const int File::READ = O_RDONLY;
const int File::READWRITE = O_RDWR;
const int File::CREATE = O_RDWR | O_CREAT;
const int File::OVERWRITE = O_RDWR | O_CREAT | O_TRUNC;
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
      throw RuntimeError() << "Cannot open file " << file << ": " << Errno;
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
    throw RuntimeError() << "Unable to obtain size of " << name
                         << ": " << Errno;
  }
  return static_cast<size_t>(statbuf.st_size);
}

const char* File::cname() const {
  return name.c_str();
}


void File::resize(size_t newsize) {
  int ret = ftruncate(fd, static_cast<off_t>(newsize));
  if (ret == -1) {
    throw RuntimeError() << "Unable to truncate() file " << name
                         << " to size " << newsize << ": " << Errno;
  }
  statbuf.st_size = -1;  // force reload stats on next request
}


void File::assert_is_not_dir() const {
  load_stats();
  if (S_ISDIR(statbuf.st_mode)) {
    throw RuntimeError() << "File " << name << " is a directory";
  }
}

void File::load_stats() const {
  if (statbuf.st_size >= 0) return;
  int ret = fstat(fd, &statbuf);
  if (ret == -1) {
    throw RuntimeError() << "Error in fstat() for file " << name
                         << ": " << Errno;
  }
}

void File::remove(const std::string& name, bool except) {
  int ret = ::remove(name.c_str());
  if (ret == -1) {
    if (except) {
      throw RuntimeError() << "Unable to remove file " << name
                           << ": " << Errno;
    } else {
      printf("Unable to remove file %s: [errno %d] %s",
             name.c_str(), errno, strerror(errno));
    }
  }
}
