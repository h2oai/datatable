//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include <cstdlib>   // std::abs
#include <cstring>   // std::memcpy
#include "csv/toa.h"
#include "utils/exceptions.h"
#include "write/value_writer.h"
namespace dt {
namespace write {



template <size_t N, typename T, void(*WriteValue)(T, writing_context&)>
class generic_writer : public value_writer {
  public:
    generic_writer(const Column& col) : value_writer(col, N) {}

    void write_normal(size_t row, writing_context& ctx) const override {
      T value;
      bool isvalid = column.get_element(row, &value);
      if (isvalid) {
        WriteValue(value, ctx);
      } else {
        ctx.write_na();
      }
    }

    void write_quoted(size_t row, writing_context& ctx) const override {
      T value;
      bool isvalid = column.get_element(row, &value);
      if (isvalid) {
        *ctx.ch++ = '"';
        WriteValue(value, ctx);
        *ctx.ch++ = '"';
      }
      else {
        ctx.write_na();
      }
    }
};



//------------------------------------------------------------------------------
// boolean writers
//------------------------------------------------------------------------------

static void write_bool01(int8_t value, writing_context& ctx) {
  *ctx.ch = static_cast<char>(value + '0');
  ctx.ch++;
}

static void write_boolTF(int8_t value, writing_context& ctx) {
  if (value) {
    std::memcpy(ctx.ch, "True", 4);
    ctx.ch += 4;
  } else {
    std::memcpy(ctx.ch, "False", 5);
    ctx.ch += 5;
  }
}

// 0/1 -> 1
// True/False -> 5
using boolean01_writer = generic_writer<1, int8_t, write_bool01>;
using booleanTF_writer = generic_writer<5, int8_t, write_boolTF>;




//------------------------------------------------------------------------------
// integer writers (decimal)
//------------------------------------------------------------------------------

static void write_int8(int8_t value, writing_context& ctx) {
  toa<int8_t>(&ctx.ch, value);
}

static void write_int16(int16_t value, writing_context& ctx) {
  toa<int16_t>(&ctx.ch, value);
}

static void write_int32(int32_t value, writing_context& ctx) {
  toa<int32_t>(&ctx.ch, value);
}

static void write_int64(int64_t value, writing_context& ctx) {
  toa<int64_t>(&ctx.ch, value);
}


// -100 -> 4
// -32767 -> 6
// -2147483647 -> 11
// -9223372036854775807 -> 20
using int8_dec_writer  = generic_writer<4,  int8_t,  write_int8>;
using int16_dec_writer = generic_writer<6,  int16_t, write_int16>;
using int32_dec_writer = generic_writer<11, int32_t, write_int32>;
using int64_dec_writer = generic_writer<20, int64_t, write_int64>;




//------------------------------------------------------------------------------
// float writers (decimal)
//------------------------------------------------------------------------------

static void write_float32(float value, writing_context& ctx) {
  toa<float>(&ctx.ch, value);
}

static void write_float64(double value, writing_context& ctx) {
  toa<double>(&ctx.ch, value);
}

// -1.23456789e+37 -> 15
// -1.1234567890123457e+307 -> 24
using float32_dec_writer = generic_writer<15, float,  write_float32>;
using float64_dec_writer = generic_writer<24, double, write_float64>;




//------------------------------------------------------------------------------
// float writers (hexadecimal)
//------------------------------------------------------------------------------

static const char hexdigits16[] = "0123456789abcdef";

static void write_float32_hex(float fvalue, writing_context& ctx) {
  uint32_t value;
  std::memcpy(&value, &fvalue, sizeof(float));
  char* ch = ctx.ch;

  if (value & F32_SIGN_MASK) {
    *ch++ = '-';
    value ^= F32_SIGN_MASK;
  }

  int exp = static_cast<int>(value >> 23);
  int subnormal = (exp == 0);
  if (exp == 0xFF) {  // nan & inf
    if (value == F32_INFINITY) {  // minus sign was already printed, if any
      std::memcpy(ch, "inf", 3);
      ctx.ch = ch + 3;
    }
    return;
  }
  uint32_t sig = (value & 0x7FFFFF);
  ch[0] = '0';
  ch[1] = 'x';
  ch[2] = static_cast<char>('1' - subnormal);
  ch[3] = '.';
  ch += 3 + (sig != 0);
  while (sig) {
    uint32_t r = sig & 0x780000;
    *ch++ = hexdigits16[r >> 19];
    sig = (sig ^ r) << 4;
  }
  exp = (exp - 127 + subnormal) & -(value != 0);
  *ch++ = 'p';
  *ch++ = '+' + (exp < 0)*('-' - '+');
  itoa(&ch, std::abs(exp));
  ctx.ch = ch;
}

static void write_float64_hex(double fvalue, writing_context& ctx) {
  uint64_t value;
  std::memcpy(&value, &fvalue, sizeof(double));
  char* ch = ctx.ch;

  if (value & F64_SIGN_MASK) {
    *ch++ = '-';
    value ^= F64_SIGN_MASK;
  }

  int exp = static_cast<int>(value >> 52);
  int subnormal = (exp == 0);
  if (exp == 0x7FF) {  // nan & inf
    if (value == F64_INFINITY) {  // minus sign was already printed, if any
      *ch++ = 'i';
      *ch++ = 'n';
      *ch++ = 'f';
      ctx.ch = ch;
    } else {
      // do not print anything for nans
    }
    return;
  }
  uint64_t sig = (value & 0xFFFFFFFFFFFFF);
  ch[0] = '0';
  ch[1] = 'x';
  ch[2] = '1' - static_cast<char>(subnormal);
  ch[3] = '.';
  ch += 3 + (sig != 0);
  while (sig) {
    uint64_t r = sig & 0xF000000000000;
    *ch++ = hexdigits16[r >> 48];
    sig = (sig ^ r) << 4;
  }
  // Add the exponent bias. Special treatment for subnormals (exp==0, value>0)
  // which should be encoded with exp=-1022, and zero (exp==0, value==0) which
  // should be encoded with exp=0.
  // `val & -flag` is equivalent to `flag? val : 0` if `flag` is 0 / 1.
  exp = (exp - 1023 + subnormal) & -(value != 0);
  *ch++ = 'p';
  *ch++ = '+' + (exp < 0)*('-' - '+');
  itoa(&ch, std::abs(exp));
  ctx.ch = ch;
}

// -0x1.123456p+120 -> 16
// -0x1.23456789ABCDEp+1022 -> 24
using float32_hex_writer = generic_writer<16, float,  write_float32_hex>;
using float64_hex_writer = generic_writer<24, double, write_float64_hex>;




//------------------------------------------------------------------------------
// string writers
//------------------------------------------------------------------------------

static inline bool character_needs_escaping(char c) {
  auto u = static_cast<unsigned char>(c);
  // Note: field separator is hard-coded as ','
  // First `u <= 44` is to give an opportunity to short-circuit early.
  return (u <= 44) && (c == ',' || c == '"' || c == '\'' || u < 32);
}

static void write_str_unquoted(CString value, writing_context& ctx) {
  const char* strstart = value.ch;
  size_t strsize = static_cast<size_t>(value.size);
  ctx.ensure_buffer_capacity(strsize);
  std::memcpy(ctx.ch, strstart, strsize);
  ctx.ch += strsize;
}

template <bool Detect, bool PrintQuotes>
static void write_str(CString value, writing_context& ctx) {
  size_t strsize = static_cast<size_t>(value.size);
  const char* strstart = value.ch;
  const char* strend = strstart + strsize;
  ctx.ensure_buffer_capacity(strsize * 2);

  char* ch = ctx.ch;
  if (strsize == 0) {
    if (PrintQuotes) {
      ch[0] = '"';
      ch[1] = '"';
      ctx.ch += 2;
    }
    return;
  }
  const char* sch = strstart;
  if (Detect && *sch != ' ' && strend[-1] != ' ') {
    while (sch < strend) {
      char c = *sch;
      if (character_needs_escaping(c)) break;
      *ch++ = c;
      sch++;
    }
  }
  if (sch < strend) {
    size_t n_chars_to_copy = static_cast<size_t>(sch - strstart);
    ch = ctx.ch;
    if (PrintQuotes) {
      *ch++ = '"';
    }
    std::memcpy(ch, strstart, n_chars_to_copy);
    ch += n_chars_to_copy;
    while (sch < strend) {
      if (*sch == '"') *ch++ = '"';  // double the quote
      *ch++ = *sch++;
    }
    if (PrintQuotes) {
      *ch++ = '"';
    }
  }
  ctx.ch = ch;
}

using string_unquoted_writer = generic_writer<0, CString, write_str_unquoted>;
using string_quotedTT_writer = generic_writer<2, CString, write_str<true, true>>;
using string_quotedFT_writer = generic_writer<2, CString, write_str<false, true>>;
using string_quotedFF_writer = generic_writer<2, CString, write_str<false, false>>;




//------------------------------------------------------------------------------
// Base value_writer
//------------------------------------------------------------------------------

value_writer::value_writer(const Column& col, size_t n)
  : column(col), max_output_size(n) {}

value_writer::~value_writer() {}


using vptr = value_writer_ptr;
vptr value_writer::create(const Column& col, const output_options& options)
{
  SType stype = col.stype();
  switch (stype) {
    case SType::VOID:
    case SType::BOOL:
      return options.booleans_as_words? vptr(new booleanTF_writer(col))
                                      : vptr(new boolean01_writer(col));
    case SType::INT8:  return vptr(new int8_dec_writer(col));
    case SType::INT16: return vptr(new int16_dec_writer(col));
    case SType::INT32: return vptr(new int32_dec_writer(col));
    case SType::INT64: return vptr(new int64_dec_writer(col));
    case SType::FLOAT32: {
      return options.floats_as_hex? vptr(new float32_hex_writer(col))
                                  : vptr(new float32_dec_writer(col));
    }
    case SType::FLOAT64: {
      return options.floats_as_hex? vptr(new float64_hex_writer(col))
                                  : vptr(new float64_dec_writer(col));
    }
    case SType::STR32:
    case SType::STR64: {
      switch (options.quoting_mode) {
        case Quoting::MINIMAL:    return vptr(new string_quotedTT_writer(col));
        case Quoting::ALL:        return vptr(new string_quotedFF_writer(col));
        case Quoting::NONNUMERIC: return vptr(new string_quotedFT_writer(col));
        case Quoting::NONE:       return vptr(new string_unquoted_writer(col));
      }
    }
    default:
      throw NotImplError() << "Cannot write values of stype " << stype;
  }
}


size_t value_writer::get_static_output_size() const {
  return max_output_size;
}

size_t value_writer::get_dynamic_output_size() const {
  return max_output_size? 0 : 10;
}



}}  // namespace dt::write
