#include <cstdio>   // std::printf
#include <cstdlib>  // std::rand, std::srand
#include <ctime>    // std::time
#include "utils.h"


int32_t* x = nullptr;
int32_t* o = nullptr;
int32_t* g = nullptr;


void prepare_data(int N, int K, int seed) {
  std::srand(seed);
  // Initialize array x
  x = new int32_t[N];
  for (int i = 0; i < N; ++i) {
    x[i] = std::rand() % K;
  }
  // Create ordering+grouping vectors
  o = new int32_t[N];
  g = new int32_t[K + 1];
  for (int i = 0; i < N; ++i) g[x[i]]++;
  int t = 0;
  for (int i = 0; i <= K; ++i) {
    int c = g[i];
    g[i] = t;
    t += c;
  }
  for (int i = 0; i < N; ++i) o[g[x[i]]++] = i;
  // Check ordering vector
  if (g[K] != N) exit(2);
  for (int i = 1; i < N; ++i) {
    if (x[o[i]] < x[o[i-1]]) {
      std::printf("Incorrect ordering at index %d: x[%d] = %d, and x[%d] = %d\n",
                  i, o[i], x[o[i]], o[i-1], x[o[i-1]]);
      exit(1);
    }
  }
}


int64_t* method1(int N, int K) {
  int64_t* res = new int64_t[K];
  for (int j = 0; j < K; ++j) {
    int i0 = g[j];
    int i1 = g[j + 1];
    int64_t sum = 0;
    for (int i = i0; i < i1; ++i) {
      int val = x[o[i]];
      sum += val;
    }
    res[j] = sum;
  }
  return res;
}

int64_t* method2(int N, int K) {
  // Compute group assignment vector
  int32_t* grass = new int32_t[N];
  for (int j = 0; j < K; ++j) {
    const int32_t* optr = o + g[j];
    const int32_t* oend = o + g[j + 1];
    while (optr < oend) grass[*optr++] = j;
  }
  // Compute gsum
  int64_t* res = new int64_t[K];
  for (int i = 0; i < N; ++i) {
    res[grass[i]] += x[i];
  }
  delete[] grass;
  return res;
}



int main(int argc, char **argv)
{
  // N - array size
  // K - number of groups
  int N = getCmdArgInt(argc, argv, "n", 10000000);
  int K = getCmdArgInt(argc, argv, "k", 1000);
  int S = getCmdArgInt(argc, argv, "seed", 0);
  if (S == 0) S = std::time(nullptr);
  std::printf("Array size n = %d\n", N);
  std::printf("Num groups k = %d\n", K);
  std::printf("Seed         = %d\n", S);
  std::printf("\n");

  std::printf("Generating data...");
  prepare_data(N, K, S);
  std::printf("ok.\n");

  std::printf("Computing with method1 (simple): ");
  double t0 = now();
  int64_t* res1 = method1(N, K);
  double t1 = now();
  std::printf("time = %g ms\n", (t1 - t0) * 1000);

  std::printf("Computing with method2 (gsum):   ");
  double t2 = now();
  int64_t* res2 = method2(N, K);
  double t3 = now();
  std::printf("time = %g ms\n", (t3 - t2) * 1000);

  std::printf("Comparing...");
  for (int i = 0; i < K; ++i) {
    if (res1[i] != res2[i]) {
      std::printf("Difference at index %d: %lld vs %lld\n", i, res1[i], res2[i]);
      exit(3);
    }
  }
  std::printf("ok.\n");

  return 1;
}
