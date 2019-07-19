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

// Ignore this warning (which should really be emitted for .h files only):
//     warning: '...' has no out-of-line virtual method definitions; its vtable
//     will be emitted in every translation unit.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"



//------------------------------------------------------------------------------
// boolean writers
//------------------------------------------------------------------------------

class boolean01_writer : public value_writer {
  public:
    boolean01_writer() : value_writer(1) {}  // 1

    void write(writing_context& ctx) const override {
      *ctx.ch = static_cast<char>(ctx.value_i32 + '0');
      ctx.ch++;
    }
};


class booleanTF_writer : public value_writer {
  public:
    booleanTF_writer() : value_writer(5) {}  // False

    void write(writing_context& ctx) const override {
      if (ctx.value_i32) {
        std::memcpy(ctx.ch, "True", 4);
        ctx.ch += 4;
      } else {
        std::memcpy(ctx.ch, "False", 5);
        ctx.ch += 5;
      }
    }
};




//------------------------------------------------------------------------------
// integer writers (decimal)
//------------------------------------------------------------------------------

class int8_dec_writer : public value_writer {
  public:
    int8_dec_writer() : value_writer(4) {}  // -100

    void write(writing_context& ctx) const override {
      toa<int8_t>(&ctx.ch, static_cast<int8_t>(ctx.value_i32));
    }
};


class int16_dec_writer : public value_writer {
  public:
    int16_dec_writer() : value_writer(6) {}  // -32767

    void write(writing_context& ctx) const override {
      toa<int16_t>(&ctx.ch, static_cast<int16_t>(ctx.value_i32));
    }
};


class int32_dec_writer : public value_writer {
  public:
    int32_dec_writer() : value_writer(11) {}  // -2147483647

    void write(writing_context& ctx) const override {
      toa<int32_t>(&ctx.ch, ctx.value_i32);
    }
};


class int64_dec_writer : public value_writer {
  public:
    int64_dec_writer() : value_writer(20) {}  // -9223372036854775807

    void write(writing_context& ctx) const override {
      toa<int64_t>(&ctx.ch, ctx.value_i64);
    }
};




//------------------------------------------------------------------------------
// float writers (decimal)
//------------------------------------------------------------------------------

class float32_dec_writer : public value_writer {
  public:
    float32_dec_writer() : value_writer(15) {}  // -1.23456789e+37

    void write(writing_context& ctx) const override {
      toa<float>(&ctx.ch, ctx.value_f32);
    }
};


class float64_dec_writer : public value_writer {
  public:
    float64_dec_writer() : value_writer(24) {}  // -1.1234567890123457e+307

    void write(writing_context& ctx) const override {
      toa<double>(&ctx.ch, ctx.value_f64);
    }
};




//------------------------------------------------------------------------------
// float writers (hexadecimal)
//------------------------------------------------------------------------------

static const char hexdigits16[] = "0123456789abcdef";

class float32_hex_writer : public value_writer {
  public:
    float32_hex_writer() : value_writer(16) {}  // -0x1.123456p+120

    void write(writing_context& ctx) const override {
      // Read the value as if it was uint32_t
      uint32_t value = static_cast<uint32_t>(ctx.value_i32);
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
};


class float64_hex_writer : public value_writer {
  public:
    float64_hex_writer() : value_writer(24) {}  // -0x1.23456789ABCDEp+1022

    void write(writing_context& ctx) const override {
      // Read the value as if it was uint64_t
      uint64_t value = static_cast<uint64_t>(ctx.value_i64);
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
};




//------------------------------------------------------------------------------
// string writers
//------------------------------------------------------------------------------

class string_unquoted_writer : public value_writer {
  public:
    string_unquoted_writer() : value_writer(0) {}

    void write(writing_context& ctx) const override {
      const char* strstart = ctx.value_str.ch;
      size_t strsize = static_cast<size_t>(ctx.value_str.size);
      ctx.ensure_buffer_capacity(strsize);
      std::memcpy(ctx.ch, strstart, strsize);
      ctx.ch += strsize;
    }

    size_t get_dynamic_output_size() const override {
      return 10;
    }
};


class string_quoted_writer : public value_writer {
  public:
    string_quoted_writer() : value_writer(2) {}

    static inline bool character_needs_escaping(char c) {
      auto u = static_cast<unsigned char>(c);
      // Note: field separator is hard-coded as ','
      // First `u <= 44` is to give an opportunity to short-circuit early.
      return (u <= 44) && (c == ',' || c == '"' || c == '\'' || u < 32);
    }

    void write(writing_context& ctx) const override {
      size_t strsize = static_cast<size_t>(ctx.value_str.size);
      const char* strstart = ctx.value_str.ch;
      const char* strend = strstart + strsize;
      ctx.ensure_buffer_capacity(strsize * 2);

      char* ch = ctx.ch;
      if (strsize == 0) {
        ch[0] = '"';
        ch[1] = '"';
        ctx.ch = ch + 2;
        return;
      }
      const char* sch = strstart;
      if (*sch != ' ' && strend[-1] != ' ') {
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
        *ch++ = '"';
        std::memcpy(ch, strstart, n_chars_to_copy);
        ch += n_chars_to_copy;
        while (sch < strend) {
          if (*sch == '"') *ch++ = '"';  // double the quote
          *ch++ = *sch++;
        }
        *ch++ = '"';
      }
      ctx.ch = ch;
    }

    size_t get_dynamic_output_size() const override {
      return 10;
    }
};




//------------------------------------------------------------------------------
// Base value_writer
//------------------------------------------------------------------------------

value_writer::value_writer(size_t n)
  : max_output_size(n) {}

value_writer::~value_writer() {}


using vptr = value_writer_ptr;
vptr value_writer::create(SType stype, const output_options& options)
{
  switch (stype) {
    case SType::BOOL:
      return options.booleans_as_words? vptr(new booleanTF_writer)
                                      : vptr(new boolean01_writer);
    case SType::INT8:  return vptr(new int8_dec_writer);
    case SType::INT16: return vptr(new int16_dec_writer);
    case SType::INT32: return vptr(new int32_dec_writer);
    case SType::INT64: return vptr(new int64_dec_writer);
    case SType::FLOAT32:
      return options.floats_as_hex? vptr(new float32_hex_writer)
                                  : vptr(new float32_dec_writer);
    case SType::FLOAT64:
      return options.floats_as_hex? vptr(new float64_hex_writer)
                                  : vptr(new float64_dec_writer);
    case SType::STR32:
    case SType::STR64:
      return vptr(new string_quoted_writer);
    default:
      throw NotImplError() << "Cannot write values of stype " << stype;
  }
}


size_t value_writer::get_static_output_size() const {
  return max_output_size;
}

size_t value_writer::get_dynamic_output_size() const {
  return 0;
}



#pragma clang diagnostic pop
}}  // namespace dt::write
