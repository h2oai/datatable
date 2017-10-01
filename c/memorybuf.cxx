#include <stdexcept>   // std::exceptions
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <stdio.h>     // remove
#include <string.h>    // strlen
#include <sys/mman.h>  // mmap
#include <sys/stat.h>  // fstat
#include <unistd.h>    // access, close, write, lseek
#include "memorybuf.h"
#include "myomp.h"
#include "utils.h"

MemoryBuffer::~MemoryBuffer() {}


//==============================================================================
// RAM-based MemoryBuffer
//==============================================================================

RamMemoryBuffer::RamMemoryBuffer(size_t n) {
  buf = malloc(n);
  allocsize = n;
  if (!buf) throw Error("Unable to allocate memory of size %zu", n);
}


RamMemoryBuffer::~RamMemoryBuffer() {
  if (owned()) {
    free(buf);
  }
}


void RamMemoryBuffer::resize(size_t n) {
  if (n == allocsize) return;
  buf = realloc(buf, n);
  if (!buf) throw Error("Unable to allocate memory of size %zu", n);
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



//==============================================================================
// FileWritableBuffer
//==============================================================================

FileWritableBuffer::FileWritableBuffer(const std::string& path)
{
  const char *cpath = path.c_str();
  fd = open(cpath, O_WRONLY|O_CREAT|O_TRUNC, 0666);
  if (fd == -1) throw Error("Cannot open file %s: Error %d %s",
                            cpath, errno, strerror(errno));
}


FileWritableBuffer::~FileWritableBuffer()
{
  if (fd != -1) close(fd);
}


size_t FileWritableBuffer::prep_write(size_t size, const void *src)
{
  // See https://linux.die.net/man/2/write
  ssize_t r = ::write(fd, static_cast<const void*>(src), size);

  if (r == -1) {
    throw Error("Error %d writing to file: %s (bytes already written: %zu)",
                errno, strerror(errno), bytes_written);
  }
  if (r < static_cast<ssize_t>(size)) {
    // This could happen if: (a) there is insufficient space on the target
    // physical medium, (b) RLIMIT_FSIZE resource limit is encountered,
    // (c) the call was interrupted by a signal handler before all data was
    // written.
    throw Error("Output to file truncated: %zd out of %zu bytes written",
                r, size);
  }
  bytes_written += size;
  return bytes_written;
}


void FileWritableBuffer::write_at(
    UNUSED(size_t pos), UNUSED(size_t size), UNUSED(const void *src))
{
  // Do nothing. FileWritableBuffer does all the writing at the "prep_write"
  // stage, because it is unable to write from multiple threads at the same
  // time (which would have required keeping multiple `fd`s on the same file
  // open, and then each thread seek-and-write to its own location.
  // Microbenchmarks show that this ends up being slower than the simple
  // single-threaded writing. Additionally, on some systems this may result in
  // lost content, if the OS decides to fill the gap with 0s when another
  // thread is writing there).
}


void FileWritableBuffer::finalize()
{
  // We could check for errors here, but I don't see the point...
  close(fd);
  fd = -1;
}



//==============================================================================
// ThreadsafeWritableBuffer
//==============================================================================

ThreadsafeWritableBuffer::ThreadsafeWritableBuffer(size_t size)
  : buffer(nullptr), allocsize(size), nlocks(0)
{}


ThreadsafeWritableBuffer::~ThreadsafeWritableBuffer()
{}


size_t ThreadsafeWritableBuffer::prep_write(size_t n, UNUSED(const void *src))
{
  size_t pos = bytes_written;
  bytes_written += n;

  // In the rare case when we need to reallocate the underlying buffer (because
  // more space is needed than originally anticipated), this will block until
  // all other threads finish writing their chunks, and only then proceed with
  // the reallocation. Otherwise reallocating the memory when some other thread
  // is writing into it leads to very-hard-to-debug crashes...
  while (bytes_written > allocsize) {
    size_t newsize = bytes_written * 2;
    int old = 0;
    // (1) wait until no other process is writing into the buffer
    while (nlocks > 0) {}
    // (2) make `numuses` negative, indicating that no other thread may
    //     initiate a memcopy operation for now.
    #pragma omp atomic capture
    { old = nlocks; nlocks -= 1000000; }
    // (3) The only case when `old != 0` is if another thread started memcopy
    //     operation in-between steps (1) and (2) above. In that case we restore
    //     the previous value of `numuses` and repeat the loop.
    //     Otherwise (and it is the most common case) we reallocate the buffer
    //     and only then restore the `numuses` variable.
    if (old == 0) {
      this->realloc(newsize);
    }
    #pragma omp atomic update
    nlocks += 1000000;
  }

  return pos;
}


void ThreadsafeWritableBuffer::write_at(size_t pos, size_t n, const void *src)
{
  int done = 0;
  while (!done) {
    int old;
    #pragma omp atomic capture
    old = nlocks++;
    if (old >= 0) {
      void *target = static_cast<void*>(static_cast<char*>(buffer) + pos);
      memcpy(target, src, n);
      done = 1;
    }
    #pragma omp atomic update
    nlocks--;
  }
}


void ThreadsafeWritableBuffer::finalize()
{
  while (nlocks > 0) {}
  while (allocsize > bytes_written) {
    while (nlocks > 0) {}
    int old = 0;
    #pragma omp atomic capture
    { old = nlocks; nlocks -= 1000000; }
    if (old == 0) {
      this->realloc(bytes_written);
    }
    #pragma omp atomic update
    nlocks += 1000000;
  }
}



//==============================================================================
// MemoryWritableBuffer
//==============================================================================

MemoryWritableBuffer::MemoryWritableBuffer(size_t size)
  : ThreadsafeWritableBuffer(size)
{
  buffer = malloc(size);
  if (!buffer) {
    throw Error("Unable to allocate memory buffer of size %zu", size);
  }
}


MemoryWritableBuffer::~MemoryWritableBuffer()
{
  free(buffer);
}


void MemoryWritableBuffer::realloc(size_t newsize)
{
  buffer = ::realloc(buffer, newsize);
  if (!buffer) throw Error("Unable to allocate memory buffer of size %zu",
                           newsize);
  allocsize = newsize;
}


void* MemoryWritableBuffer::get()
{
  void *buf = buffer;
  buffer = nullptr;
  allocsize = 0;
  return buf;
}



//==============================================================================
// MmapWritableBuffer
//==============================================================================

MmapWritableBuffer::MmapWritableBuffer(const std::string& path, size_t size)
  : ThreadsafeWritableBuffer(size), filename(path)
{
  const char *c_name = filename.c_str();
  int fd = open(c_name, O_RDWR|O_CREAT, 0666);
  if (fd == -1) throw Error("Cannot open file %s: Error %d %s",
                            c_name, errno, strerror(errno));

  lseek(fd, static_cast<off_t>(size), SEEK_SET);
  ::write(fd, static_cast<void*>(&fd), 1);  // write 1 byte, doesn't matter what

  buffer = mmap(NULL, size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (buffer == MAP_FAILED) {
    throw Error("Memory map failed, error %d: %s", errno, strerror(errno));
  }
}


MmapWritableBuffer::~MmapWritableBuffer()
{
  munmap(buffer, allocsize);
}


void MmapWritableBuffer::realloc(size_t newsize)
{
  munmap(buffer, allocsize);

  const char *c_fname = filename.c_str();
  truncate(c_fname, static_cast<off_t>(newsize));

  int fd = open(c_fname, O_RDWR);
  buffer = mmap(NULL, newsize, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (!buffer) throw Error("Unable to resize the file to %s",
                           filesize_to_str(newsize));
  allocsize = newsize;
}
