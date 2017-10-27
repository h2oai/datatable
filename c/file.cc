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
#include "file.h"
#include <errno.h>      // errno
#include <fcntl.h>      // open
#include <stdio.h>      // remove
#include <string.h>     // strerror
#include <sys/stat.h>   // fstat
#include <unistd.h>     // close, write, ftruncate
#include "utils.h"


const int File::READ = O_RDONLY;
const int File::READWRITE = O_RDWR;
const int File::CREATE = O_RDWR | O_CREAT;
const int File::OVERWRITE = O_RDWR | O_CREAT | O_TRUNC;


//------------------------------------------------------------------------------

File::File(const std::string& file)
    : File(file, READ, 0) {}

File::File(const std::string& file, int flags, mode_t mode) {
  fd = open(file.c_str(), flags, mode);
  if (fd == -1) {
    throw Error("Cannot open file %s: [errno %d] %s",
                file.c_str(), errno, strerror(errno));
  }
  name = file;
  // size of -1 is used to indicate that `statbuf` was not loaded yet
  statbuf.st_size = -1;
}

File::~File() {
  if (fd != -1) {
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
size_t File::asize(const std::string& filename) {
  struct stat statbuf;
  int ret = stat(filename.c_str(), &statbuf);
  if (ret == -1) {
    throw Error("Unable to obtain size of %s: [errno %d] %s",
                filename.c_str(), errno, strerror(errno));
  }
  return static_cast<size_t>(statbuf.st_size);
}

const char* File::cname() const {
  return name.c_str();
}


void File::resize(size_t newsize) {
  int ret = ftruncate(fd, static_cast<off_t>(newsize));
  if (ret == -1) {
    throw Error("Unable to truncate() file %s to size %zu: [errno %d] %s",
                cname(), newsize, errno, strerror(errno));
  }
  statbuf.st_size = -1;  // force reload stats on next request
}


void File::assert_is_not_dir() const {
  load_stats();
  if (S_ISDIR(statbuf.st_mode)) {
    throw Error("File %s is a directory", name.c_str());
  }
}

void File::load_stats() const {
  if (statbuf.st_size >= 0) return;
  int ret = fstat(fd, &statbuf);
  if (ret == -1) {
    throw Error("Error in fstat() for file %s: [errno %d] %s",
                name.c_str(), errno, strerror(errno));
  }
}

void File::remove(const std::string& filename, bool except) {
  int ret = ::remove(filename.c_str());
  if (ret == -1) {
    if (except) {
      throw Error("Unable to remove file %s: [errno %d] %s",
                  filename.c_str(), errno, strerror(errno));
    } else {
      printf("Unable to remove file %s: [errno %d] %s",
             filename.c_str(), errno, strerror(errno));
    }
  }
}
