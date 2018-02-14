#ifndef FLOAT64_h
#define FLOAT64_h
#include <stdint.h>
#include "main.h"


class Float64BenchmarkSuite : public BenchmarkSuite {

  public:
    Float64BenchmarkSuite();

    const char* name() override { return "float64"; }
    std::string repr() override;
};

#endif
