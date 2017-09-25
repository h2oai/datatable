#include <errno.h>    // errno
#include <stdio.h>    // printf, sprintf
#include <stdlib.h>   // srand, rand
#include <string.h>   // strerror, strlen
#include <fcntl.h>    // O_WRONLY, O_CREAT
#include <sys/mman.h> // mmap
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
  int nth = omp_get_num_threads();

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
  int nth = omp_get_num_threads();

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

  int nth = omp_get_num_threads();
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
}


static WKernel kernels[] = {
  {&kernel_write,  "write"},
  {&kernel_seek,   "seek&write"},
  {&kernel_mmap,   "memory-map"},
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
    double total_time = 0;
    for (int b = 0; b < B; b++) {
      char filename[20];
      sprintf(filename, "out-%d-%d.csv", k, b);
      double t0 = now();
      kernels[k].kernel(filename, data, N);
      double t1 = now();
      total_time += t1 - t0;
      // remove(filename);
    }
    printf("[%d] %-*s: %7.3f s\n",
           k, maxnamelen, kernels[k].name, total_time/B);
  }

  delete[] data;
}
