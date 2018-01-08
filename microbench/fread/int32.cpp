#include "int32.h"
#include <cstdlib>  // strtol
#include <sstream>  // ostringstream

static const int32_t NA_INT32 = -2147483648;


// Based on standard C `strtol` function; does not do proper error checking
// (but they could be implemented if needed).
static void parse_strtol(ParseContext& ctx) {
  char* ch = nullptr;
  long int value = strtol(ctx.ch, &ch, 10);
  ctx.ch = ch;
  ctx.target->int32 = (int32_t) value;
}


// Standard fread implementation
static void parser_fread(ParseContext& ctx) {
  const char* ch = ctx.ch;
  bool negative = (*ch == '-');
  ch += (negative || *ch == '+');
  const char* start = ch;
  uint_fast64_t acc = 0;
  uint_fast8_t digit;

  while (*ch=='0') ch++;
  uint_fast32_t sf = 0;
  while ( (digit = (uint_fast8_t)(ch[sf]-'0')) < 10 ) {
    acc = 10*acc + digit;
    sf++;
  }
  ch += sf;
  if ((sf || ch>start) && sf<=10 && acc<=INT32_MAX) {
    ctx.target->int32 = negative ? -(int32_t)acc : (int32_t)acc;
    ctx.ch = ch;
  } else {
    ctx.target->int32 = NA_INT32;
  }
}


// similar to basic fread approach, but use `uint32_t`s everywhere
static void parser_fread32(ParseContext& ctx) {
  const char* ch = ctx.ch;
  bool negative = (*ch == '-');
  ch += (negative || *ch == '+');
  const char* start = ch;
  uint32_t acc = 0, sf = 0, prev, digit;

  while (*ch=='0') ch++;
  while ((digit = static_cast<uint32_t>(ch[sf]-'0')) < 10) {
    prev = acc;
    acc = 10*acc + digit;
    sf++;
  }
  ch += sf;
  if ((sf? sf < 10 : ch > start) ||
      (sf == 10 && prev < 214748364) ||
      (sf == 10 && prev == 214748364 && ch[-1] <= '7')) {
    ctx.target->int32 = negative? -(int32_t)acc : (int32_t)acc;
    ctx.ch = ch;
  } else {
    ctx.target->int32 = NA_INT32;
  }
}


// Same as fread32, but use bit hacks to negate the final value
static void parser_fread32neg(ParseContext& ctx) {
  const char* ch = ctx.ch;
  bool negative = (*ch == '-');
  ch += (negative || *ch == '+');
  const char* start = ch;
  uint32_t acc = 0, sf = 0, prev, digit;

  while (*ch=='0') ch++;
  while ((digit = static_cast<uint32_t>(ch[sf]-'0')) < 10) {
    prev = acc;
    acc = 10*acc + digit;
    sf++;
  }
  ch += sf;
  if ((sf? sf < 10 : ch > start) ||
      (sf == 10 && prev < 214748364) ||
      (sf == 10 && prev == 214748364 && ch[-1] <= '7')) {
    int32_t v = (int32_t)acc;
    ctx.target->int32 = (v ^ -negative) + negative;
    ctx.ch = ch;
  } else {
    ctx.target->int32 = NA_INT32;
  }
}


// Simplest approach, but does not do any error-checking
static void parser_naive(ParseContext& ctx) {
  const char* p = ctx.ch;
  int32_t x = 0;
  bool neg = false;
  if (*p == '-') {
    neg = true;
    ++p;
  }
  while (*p >= '0' && *p <= '9') {
    x = (x*10) + (*p - '0');
    ++p;
  }
  if (neg) {
    x = -x;
  }
  ctx.ch = p;
  ctx.target->int32 = x;
}



//------------------------------------------------------------------------------

Int32BenchmarkSuite::Int32BenchmarkSuite() {
  ncols = 10;
  targets = new field64[ncols];
  input_str = "2147483647,0,-2000000000,2490579,23,16,-7,891393,999,10000,";
  add_kernel(ParseKernel("strtol", parse_strtol));
  add_kernel(ParseKernel("naive", parser_naive));
  add_kernel(ParseKernel("fread0", parser_fread));
  add_kernel(ParseKernel("fread32", parser_fread32));
  add_kernel(ParseKernel("fread32neg", parser_fread32neg));
}


std::string Int32BenchmarkSuite::repr() {
  std::ostringstream out;
  for (int i = 0; i < ncols; ++i) {
    if (i) out << ',';
    out << targets[i].int32;
  }
  return out.str();
}
