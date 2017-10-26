#include <cstdlib>
#include <stdio.h>
#include <chrono>
#include <cstring>
#include <cinttypes>

size_t padding(size_t datasize) {
  return ((8 - ((datasize + sizeof(int32_t)) & 7)) & 7) + sizeof(int32_t);
}
using namespace std::chrono;

int main(int argc, const char* argv[]) {
  int seed = 122493;
  double tsum0 = 0;
  double tsum1 = 0;
  int nrows = 10000000;
  int iter_count = 10;
  int str_min = 0;
  int str_max = 5;
  if (argc > 1) nrows = atoi(argv[1]);
  if (argc > 2) iter_count = atoi(argv[2]);
  if (argc > 3) seed = atoi(argv[3]);
  srand(seed);
  int str_range = str_max - str_min + 1 + 1; // add an extra 1 for NAs
  int na_count = 0;
  for (int i = 0; i < iter_count; ++i) {
    char *data = (char*) malloc((size_t) str_max * nrows);
    char *data_root = data;
    int32_t *offsets = (int32_t*) malloc(sizeof(int32_t) * nrows);
    int32_t prev_off = 1;
    for (int ii = 0; ii < nrows; ++ii) {
      int str_len = (rand() % str_range) + str_min;
      if (str_len > str_max) {
        offsets[ii] = (-prev_off);
        ++na_count;
      } else {
        for (int j = 0; j < str_len; ++j) {
          *data = ('a' + (char) (rand() % 26));
          ++data;
        }
        prev_off += str_len;
        offsets[ii] = prev_off;
      }
    }
    size_t datasize0 = (size_t) prev_off - 1;
    size_t pad_size0 = padding(datasize0);
    size_t offoff0 = datasize0 + pad_size0;
    data_root = (char*) realloc(data_root, datasize0 + pad_size0 + sizeof(int32_t) * (size_t) nrows);
    memset(data_root + datasize0, 0xFF, pad_size0);
    memcpy(data_root + offoff0, offsets, sizeof(int32_t) * nrows);
    delete offsets;

    printf("Iteration %d (%d rows, %d NAs)\n", i + 1, nrows, na_count);
    printf("Starting copy reify...");
    size_t offoff;
    size_t new_mbuf_size;
    steady_clock::time_point t0 = steady_clock::now();
    int32_t *offs1 = (int32_t*) (data_root + offoff0);
    int32_t *offs0 = offs1 - 1;
    int32_t datasize_cast = 0;
    for (int64_t i = 0; i < nrows; ++i) {
      if (offs1[i] > 0) {
        datasize_cast += offs1[i] - abs(offs0[i]);
      }
    }
    size_t datasize = static_cast<size_t>(datasize_cast);
    size_t pad_size = padding(datasize);
    offoff = datasize + pad_size;
    new_mbuf_size = offoff + static_cast<size_t>(nrows) * sizeof(int32_t);
    char *new_mbuf = (char*) malloc(new_mbuf_size);
    int32_t *new_offs = static_cast<int32_t*>((void*) (new_mbuf + offoff));
    char *strs = data_root - 1;
    char *data_dest = static_cast<char*>(new_mbuf);
    prev_off = 1;
    for (int64_t i = 0; i < nrows; ++i) {
      if (offs1[i] > 0) {
        int32_t off0 = abs(offs0[i]);
        int32_t str_len = offs1[i] - off0;
        if (str_len != 0) {
          memcpy(data_dest, strs + off0, static_cast<size_t>(str_len));
          data_dest += str_len;
          prev_off += str_len;
        }
        *new_offs = prev_off;
        ++new_offs;
      } else {
        *new_offs = -prev_off;
        ++new_offs;
      }
    }
    memset(new_mbuf + datasize, 0xFF, pad_size);
    double t0_out = duration_cast<duration<double>>(steady_clock::now() - t0).count();
    tsum0 += t0_out;
    printf("done in %f seconds\n", t0_out);
    delete new_mbuf;
    new_mbuf = data_root;
    printf("Starting inplace reify...");
    steady_clock::time_point t1 = steady_clock::now();
    offs1 = (int32_t*) (data_root + offoff);
    offs0 = offs1 - 1;
    strs = data_root - 1;
    data_dest = static_cast<char*>(new_mbuf);
    int32_t nrows_cast = static_cast<int32_t>(nrows);
    for (int32_t i = 0; i < nrows_cast; ++i) {
      if (offs1[i] > 0) {
        int32_t off0 = abs(offs0[i]);
        int32_t str_len = offs1[i] - off0;
        if (str_len != 0) {
          memmove(data_dest, strs + off0, static_cast<size_t>(str_len));
          data_dest += str_len;
        }
      }
    }
    datasize = static_cast<size_t>(
        data_dest - static_cast<char*>(new_mbuf));
    pad_size = padding(datasize);
    offoff = datasize + pad_size;
    new_mbuf_size = offoff + static_cast<size_t>(nrows) * sizeof(int32_t);
    new_offs = static_cast<int32_t*>((void*) (new_mbuf + offoff));
    prev_off = 1;
    for (int32_t i = 0; i < nrows_cast; ++i) {
      if (offs1[i] > 0) {
        int32_t off0 = abs(offs0[i]);
        prev_off += offs1[i] - off0;
        new_offs[i] = prev_off;
      } else {
        new_offs[i] = -prev_off;
      }
    }
    memset(new_mbuf + datasize, 0xFF, pad_size);
    double t1_out = duration_cast<duration<double>>(steady_clock::now() - t1).count();
    tsum1 += t1_out;
    printf("done in %f seconds\n", t1_out);
    printf("\n");
    delete data_root;
  }
  printf("COMPLETE\n");
  printf("copy mean: %f\ninplace mean: %f\n", tsum0 / iter_count, tsum1 / iter_count);
}
