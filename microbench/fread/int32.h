#ifndef INT32_H
#define INT32_H
#include <stdint.h>
#include "main.h"


class Int32BenchmarkSuite : public BenchmarkSuite {

  public:
    Int32BenchmarkSuite();

    const char* name() override { return "int32"; }
    std::string repr() override;
};

#endif
