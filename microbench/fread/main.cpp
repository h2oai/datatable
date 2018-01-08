#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <stdexcept>
#include "main.h"
#include "int32.h"
#include "utils.h"


//------------------------------------------------------------------------------
// BenchmarkSuite
//------------------------------------------------------------------------------

BenchmarkSuite::BenchmarkSuite() {
  maxnamelen = 0;
  ncols = 0;
  targets = NULL;
}

BenchmarkSuite::~BenchmarkSuite() {
  delete[] targets;
}

BenchmarkSuite* BenchmarkSuite::create(int A) {
  switch (A) {
    case 1: return new Int32BenchmarkSuite();
  }
  throw std::runtime_error("Unknown benchmark");
}


void BenchmarkSuite::add_kernel(ParseKernel kernel) {
  kernels.push_back(kernel);
  int namelen = kernel.name.size();
  if (namelen > maxnamelen) maxnamelen = namelen;
}


void BenchmarkSuite::run(int N)
{
  const char* input = input_str.data();
  const char* ch = input;
  ParseContext ctx = {
    .ch = ch,
    .target = targets
  };

  for (int k = 0; k < kernels.size(); k++) {
    const char* kname = kernels[k].name.data();
    parser_fn kernel = kernels[k].parser;

    double t0 = now();
    int i, j;
    for (i = 0; i < N; i++) {
      ch = input;
      ctx.target = targets;
      for (j = 0; j < ncols; ++j) {
        kernel(ctx);
        if (*ch != ',') goto end;
        ch++;
        ctx.target++;
      }
    }
    end:
    if (i == N) {
      double t1 = now();
      std::string out = repr();
      printf("[%d] %-*s: %7.3f ns  out=[%s]\n",
             k, maxnamelen, kname, (t1-t0)*1e9/N/ncols, out.data());
    } else {
      printf("[%d] %-*s: failed to parse input at i=%d, j=%d, ch=+%ld (%s)\n",
             k, maxnamelen, kname, i, j, ch - input, ch);
    }
  }
}


//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
  int A = getCmdArgInt(argc, argv, "parser", NPARSERS);
  int N = getCmdArgInt(argc, argv, "n", 1000000);
  if (A <= 0 || A > NPARSERS) {
    printf("Invalid parser: %d  (max parsers=%d)\n", A, NPARSERS);
    return 1;
  }

  BenchmarkSuite* bs = BenchmarkSuite::create(A);

  printf("Parser = %s\n", bs->name());
  printf("Nrows  = %d\n", N);
  printf("\n");
  bs->run(N);

  return 0;
}
