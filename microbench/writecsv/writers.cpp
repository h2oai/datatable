#include <stdexcept>  // std::exceptions
#include <errno.h>    // errno
#include <fcntl.h>    // open
#include <stdio.h>    // printf, sprintf
#include <stdlib.h>   // srand, rand
#include <string.h>   // strerror, strlen
#include <fcntl.h>    // O_WRONLY, O_CREAT
#include <sys/mman.h> // mmap
#include <sys/stat.h> // fstat
#include <time.h>     // time
#include <unistd.h>   // write, close, lseek
#include <omp.h>
#include "writecsv.h"

static const int64_t DIVS[19] = {
  1L,
  10L,
  100L,
  1000L,
  10000L,
  100000L,
  1000000L,
  10000000L,
  100000000L,
  1000000000L,
  10000000000L,
  100000000000L,
  1000000000000L,
  10000000000000L,
  100000000000000L,
  1000000000000000L,
  10000000000000000L,
  100000000000000000L,
  1000000000000000000L,
};
static void write_int64(char **pch, int64_t value) {
  if (value == 0) {
    *((*pch)++) = '0';
    return;
  }
  char *ch = *pch;
  if (value < 0) {
    *ch++ = '-';
    value = -value;
  }
  int r = (value < 1000000)? 5 : 18;
  for (; value < DIVS[r]; r--);
  for (; r; r--) {
    int d = value / DIVS[r];
    *ch++ = d + '0';
    value -= d * DIVS[r];
  }
  *ch = value + '0';
  *pch = ch + 1;
}




// Plain write into the file from within the #ordered section.
void kernel_write(const char *filename, int64_t *data, int64_t nrows)
{
  int fd = open(filename, O_WRONLY|O_CREAT, 0666);
  if (fd == -1) {
    printf("Unable to create file %s: %s", filename, strerror(errno));
    return;
  }
  int64_t rows_per_chunk = 20000;
  int64_t nchunks = nrows / rows_per_chunk;
  int64_t bytes_per_chunk = rows_per_chunk * 5 * 20;
  int nth = omp_get_max_threads();

  bool stopTeam = false;
  #pragma omp parallel num_threads(nth)
  {
    char *mybuff = new char[bytes_per_chunk];
    #pragma omp for ordered schedule(dynamic)
    for (int64_t start = 0; start < nrows; start += rows_per_chunk) {
      if (stopTeam) continue;
      int64_t end = start + rows_per_chunk;
      if (end > nrows) end = nrows;
      char *mych = mybuff;
      for (int64_t i = start; i < end; i++) {
        for (int64_t j = 0; j < 5; j++) {
          write_int64(&mych, data[i] + j);
          *mych++ = ',';
        }
        mych[-1] = '\n';
      }
      #pragma omp ordered
      {
        int64_t size = mych - mybuff;
        int ret = write(fd, mybuff, size);
        if (ret == -1) {
          stopTeam = true;
          printf("Error writing a buffer to file\n");
        }
      }
    }
    delete[] mybuff;
  }
  // Finished
  close(fd);
}


// Similar to kernel_write, but each thread has its own file descriptor.
// It will seek to the proper position in the file and write there.
void kernel_seek(const char *filename, int64_t *data, int64_t nrows)
{
  int fd = open(filename, O_WRONLY|O_CREAT, 0666);
  if (fd == -1) {
    printf("Unable to create file %s: %s", filename, strerror(errno));
    return;
  }
  close(fd);
  int64_t rows_per_chunk = 20000;
  int64_t nchunks = nrows / rows_per_chunk;
  int64_t bytes_per_chunk = rows_per_chunk * 5 * 20;
  int64_t bytes_written = 0;
  int nth = omp_get_max_threads();

  bool stopTeam = false;
  #pragma omp parallel num_threads(nth)
  {
    int fd = open(filename, O_WRONLY);
    char *mybuff = new char[bytes_per_chunk];
    int64_t write_size = 0;
    int64_t write_at = 0;

    #pragma omp for ordered schedule(dynamic)
    for (int64_t start = 0; start < nrows; start += rows_per_chunk) {
      if (stopTeam) continue;
      if (write_size) {
        lseek(fd, write_at, SEEK_SET);
        int ret = write(fd, mybuff, write_size);
        write_size = 0;
      }

      int64_t end = start + rows_per_chunk;
      if (end > nrows) end = nrows;
      char *mych = mybuff;
      for (int64_t i = start; i < end; i++) {
        for (int64_t j = 0; j < 5; j++) {
          write_int64(&mych, data[i] + j);
          *mych++ = ',';
        }
        mych[-1] = '\n';
      }
      #pragma omp ordered
      {
        write_size = mych - mybuff;
        write_at = bytes_written;
        bytes_written += write_size;
      }
    }
    if (write_size) {
      lseek(fd, write_at, SEEK_SET);
      int ret = write(fd, mybuff, write_size);
    }

    close(fd);
    delete[] mybuff;
  }
}


void kernel_mmap(const char *filename, int64_t *data, int64_t nrows)
{
  int64_t bytes_total = nrows * 5 * 20;
  int64_t rows_per_chunk = 20000;
  int64_t nchunks = nrows / rows_per_chunk;
  int64_t bytes_per_chunk = bytes_total / nchunks;

  int fd = open(filename, O_RDWR|O_CREAT, 0666);
  if (fd == -1) {
    printf("Unable to create file %s: %s", filename, strerror(errno));
    return;
  }
  size_t allocsize = static_cast<size_t>(bytes_total * 1.25);
  lseek(fd, allocsize, SEEK_SET);
  write(fd, filename, 1);  // doesn't matter what to write

  char *buf = static_cast<char*>(mmap(NULL, allocsize, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0));
  close(fd);
  if (buf == MAP_FAILED) {
    printf("Memory map failed!\n");
    return;
  }
  size_t bytes_written = 0;

  int nth = omp_get_max_threads();
  #pragma omp parallel num_threads(nth)
  {
    char *mybuff = new char[bytes_per_chunk];
    int64_t write_size = 0;
    int64_t write_at = 0;

    #pragma omp for ordered schedule(dynamic)
    for (int64_t start = 0; start < nrows; start += rows_per_chunk) {
      if (write_size) {
        memcpy(buf + write_at, mybuff, write_size);
        write_size = 0;
      }

      int64_t end = start + rows_per_chunk;
      if (end > nrows) end = nrows;
      char *mych = mybuff;
      for (int64_t i = start; i < end; i++) {
        for (int64_t j = 0; j < 5; j++) {
          write_int64(&mych, data[i] + j);
          *mych++ = ',';
        }
        mych[-1] = '\n';
      }
      #pragma omp ordered
      {
        write_size = mych - mybuff;
        write_at = bytes_written;
        bytes_written += write_size;
      }
    }
    if (write_size) {
      memcpy(buf + write_at, mybuff, write_size);
    }

    delete[] mybuff;
  }
  munmap(buf, allocsize);
  truncate(filename, bytes_written);
}


void kernel_mmap2(const char *filename, int64_t *data, int64_t nrows)
{
  int64_t bytes_total = nrows * 5 * 20;
  int64_t rows_per_chunk = 20000;
  int64_t nchunks = nrows / rows_per_chunk;
  int64_t bytes_per_chunk = bytes_total / nchunks;
  size_t allocsize = static_cast<size_t>(bytes_total * 1.25);

  double t0 = now();
  FILE *fp = fopen(filename, "w");
  fseek(fp, allocsize - 1, SEEK_SET);
  fputc('\0', fp);
  fclose(fp);

  int fd = open(filename, O_RDWR, 0666);
  if (fd == -1) throw std::runtime_error("Cannot open file");
  struct stat statbuf;
  if (fstat(fd, &statbuf) == -1) throw std::runtime_error("Error in fstat()");
  if (S_ISDIR(statbuf.st_mode)) throw std::runtime_error("File is a directory");
  allocsize = (size_t) statbuf.st_size;

  char *buf = static_cast<char*>(mmap(NULL, allocsize, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0));
  close(fd);
  if (buf == MAP_FAILED) throw std::runtime_error("Memory map failed!\n");
  size_t bytes_written = 0;

  double t1 = now();
  int nth = omp_get_max_threads();
  printf("      Using n threads = %d\n", nth);
  #pragma omp parallel num_threads(nth)
  {
    char *thbuf = new char[bytes_per_chunk];
    char *tch = thbuf;
    int64_t th_write_size = 0;
    int64_t th_write_at = 0;

    #pragma omp for ordered schedule(dynamic)
    for (int64_t i = 0; i < nchunks; i++) {
      int64_t row0 = static_cast<int64_t>(i * rows_per_chunk);
      int64_t row1 = static_cast<int64_t>((i + 1) * rows_per_chunk);
      if (i == nchunks-1) row1 = nrows;  // always go to the last row for last chunk

      if (th_write_size) {
        memcpy(buf + th_write_at, thbuf, th_write_size);
        tch = thbuf;
        th_write_size = 0;
      }

      for (int64_t row = row0; row < row1; row++) {
        for (int64_t col = 0; col < 5; col++) {
          write_int64(&tch, data[row] + col);
          *tch++ = ',';
        }
        tch[-1] = '\n';
      }
      #pragma omp ordered
      {
        th_write_at = bytes_written;
        th_write_size = static_cast<size_t>(tch - thbuf);
        bytes_written += th_write_size;
      }
    }
    if (th_write_size) {
      memcpy(buf + th_write_at, thbuf, th_write_size);
    }

    delete[] thbuf;
  }
  double t2 = now();
  munmap(buf, allocsize);
  truncate(filename, bytes_written);
  double t3 = now();
  printf("        %6.3fs Creating file\n", t1-t0);
  printf("      + %6.3fs Writing data\n", t2-t1);
  printf("      + %6.3fs Finalizing\n", t3-t2);
  printf("      = %6.3fs Total\n", t3-t0);
}


static WKernel kernels[] = {
  {&kernel_mmap,   "memorymap1"},
  {&kernel_mmap2,  "memorymap2"},
  {&kernel_write,  "write"},
  {&kernel_seek,   "seek&write"},
  {NULL, NULL}
};


//=================================================================================================
// Main
//
// This function tests different methods of writing to a file. The data being written is not
// relevant. We will be writing int64s, because they are roughly in the middle (time-wise) compared
// to other types (the longest is double, at 200ns).
//=================================================================================================

void test_write_methods(int B, int64_t N)
{
  // First, create the dataset
  srand(time(NULL));
  int64_t *data = new int64_t[N];
  for (int64_t i = 0; i < N; i++) {
    int64_t x = static_cast<int64_t>(rand());
    int64_t y = static_cast<int64_t>(rand());
    data[i] = (x << 32) + y;
  }

  int nkernels = 0, maxnamelen = 0;
  while (kernels[nkernels].kernel) {
    int len = strlen(kernels[nkernels].name);
    if (len > maxnamelen) maxnamelen = len;
    nkernels++;
  }

  for (int k = 0; k < nkernels; k++) {
    printf("[%d] %s:\n", k, kernels[k].name);
    for (int b = 0; b < B; b++) {
      char filename[20];
      sprintf(filename, "out-%d-%d.csv", k, b);
      double t0 = now();
      kernels[k].kernel(filename, data, N);
      double t1 = now();

      int fd = open(filename, O_RDONLY);
      struct stat statbuf;
      fstat(fd, &statbuf);
      printf("  %7.3fs   %lld B\n", t1-t0, statbuf.st_size);
      close(fd);
      remove(filename);

      // Drop caches
      sync();
      fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
      char data = '3';
      write(fd, &data, 1);
      close(fd);
    }
  }

  delete[] data;
}
