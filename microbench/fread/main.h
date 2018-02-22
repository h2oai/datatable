#ifndef FREAD_H
#define FREAD_H
#include <stdint.h>
#include <string>
#include <vector>


extern double now();

#define NPARSERS 2


union field64 {
  int8_t   int8;
  int32_t  int32;
  int64_t  int64;
  float    float32;
  double   float64;
};

struct ParseContext {
  const char*& ch;
  field64* target;
};

typedef void (*parser_fn)(ParseContext&);


struct ParseKernel {
  std::string name;
  parser_fn parser;

  ParseKernel(std::string n, parser_fn p): name(n), parser(p) {}
};


class BenchmarkSuite {
  private:
    std::vector<ParseKernel> kernels;
    int maxnamelen;

  protected:
    std::string input_str;
    field64* targets;
    int ncols;

  public:
    BenchmarkSuite();
    virtual ~BenchmarkSuite();
    static BenchmarkSuite* create(int A);
    void add_kernel(ParseKernel kernel);

    void run(int N);
    virtual const char* name() = 0;
    virtual std::string repr() = 0;
};


#endif
