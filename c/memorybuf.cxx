#include <stdexcept>   // std::exceptions
#include <fcntl.h>     // open
#include <stdio.h>     // remove
#include <string.h>    // strlen
#include <sys/mman.h>  // mmap
#include <sys/stat.h>  // fstat
#include <unistd.h>    // access, close
#include "memorybuf.h"

MemoryBuffer::~MemoryBuffer() {}


//==============================================================================
// RAM-based MemoryBuffer
//==============================================================================

RamMemoryBuffer::RamMemoryBuffer(size_t n) {
  buf = malloc(n);
  if (!buf) throw std::runtime_error("Unable to allocate memory buffer");
}


RamMemoryBuffer::~RamMemoryBuffer() {
  if (owned()) {
    free(buf);
  }
}


void RamMemoryBuffer::resize(size_t n) {
  if (n == allocsize) return;
  buf = realloc(buf, n);
  allocsize = n;
}



//==============================================================================
// String MemoryBuffer
//==============================================================================

StringMemoryBuffer::StringMemoryBuffer(const char *str) {
  buf = static_cast<void*>(const_cast<char*>(str));
  allocsize = strlen(str);
  flags = MB_READONLY | MB_EXTERNAL;
}

void StringMemoryBuffer::resize(size_t) {}

StringMemoryBuffer::~StringMemoryBuffer() {}



//==============================================================================
// Disk-based MemoryBuffer
//==============================================================================

MmapMemoryBuffer::MmapMemoryBuffer(const char *path, size_t n, int flags_)
{
  flags = flags_;
  filename = path;
  bool temporary = owned();
  bool isreadonly = readonly();
  bool create = (flags & MB_CREATE) == MB_CREATE;
  bool new_temp_file = create && temporary && !isreadonly;
  bool new_perm_file = create && !temporary && !isreadonly;
  bool read_file = !create && !temporary && isreadonly;
  bool readwrite_file = !create && !temporary && !isreadonly;
  if (!(new_temp_file || new_perm_file || read_file || readwrite_file)) {
    throw std::invalid_argument("invalid flags parameter");
  }

  if (create) {
    // Should we care whether the file exists or not?
    // bool file_exists = (access(filename, F_OK) != -1);
    // if (file_exists) {
    //   throw std::runtime_error("File with such name already exists");
    // }
    // Create new file of size `n`.
    FILE *fp = fopen(filename, "w");
    if (n) {
      fseek(fp, (long)(n - 1), SEEK_SET);
      fputc('\0', fp);
    }
    fclose(fp);
  } else {
    // Check that the file exists and is accessible
    int mode = isreadonly? R_OK : (R_OK|W_OK);
    bool file_accessible = (access(filename, mode) != -1);
    if (!file_accessible) {
      throw std::runtime_error("File cannot be opened");
    }
  }

  // Open the file and determine its size
  int fd = open(filename, isreadonly? O_RDONLY : O_RDWR, 0666);
  if (fd == -1) throw std::runtime_error("Cannot open file");
  struct stat statbuf;
  if (fstat(fd, &statbuf) == -1) throw std::runtime_error("Error in fstat()");
  if (S_ISDIR(statbuf.st_mode)) throw std::runtime_error("File is a directory");
  n = (size_t) statbuf.st_size;

  // Memory-map the file.
  allocsize = n;
  buf = mmap(NULL, n, PROT_WRITE|PROT_READ,
             isreadonly? MAP_PRIVATE|MAP_NORESERVE : MAP_SHARED, fd, 0);
  close(fd);  // fd is no longer needed
  if (buf == MAP_FAILED) {
    throw std::runtime_error("Memory map failed");
  }
}


MmapMemoryBuffer::~MmapMemoryBuffer() {
  munmap(buf, allocsize);
  if (owned()) {
    remove(filename);
  }
}


void MmapMemoryBuffer::resize(size_t n) {
  if (readonly()) return;
  munmap(buf, allocsize);
  truncate(filename, (off_t)n);

  int fd = open(filename, O_RDWR);
  buf = mmap(NULL, n, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (buf == MAP_FAILED) {
    throw std::runtime_error("Memory map failed");
  }
  allocsize = n;
}
