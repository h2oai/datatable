//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_FREAD_h
#define dt_FREAD_h
#include <stdint.h>  // uint32_t
#include <stdlib.h>  // size_t
#include "utils.h"
#include "utils/omp.h"
#include "memorybuf.h"
#include "csv/reader.h"

extern const long double pow10lookup[701];
extern const uint8_t hexdigits[256];
extern const uint8_t allowedseps[128];
const char* strlim(const char* ch, size_t limit);



struct FreadTokenizer {
  // Pointer to the current parsing location
  const char* ch;

  // Where to write the parsed value. The pointer will be incremented after
  // each successful read.
  field64* target;

  // Anchor pointer for string parser, this pointer is the starting point
  // relative to which `str32.offset` is defined.
  const char* anchor;

  const char* eof;

  const char* const* NAstrings;

  // what to consider as whitespace to skip: ' ', '\t' or 0 means both
  // (when sep!=' ' && sep!='\t')
  char whiteChar;

  // Decimal separator for parsing floats. The default value is '.', but
  // in some cases ',' may also be used.
  char dec;

  // Field separator
  char sep;

  // Character used for field quoting.
  char quote;

  // How the fields are quoted.
  // TODO: split quoteRule differences into separate parsers.
  int8_t quoteRule;

  // Should white space be removed?
  bool strip_whitespace;

  // Do we consider blank as NA string?
  bool blank_is_na;

  bool LFpresent;

  void skip_white();
  bool end_of_field();
  const char* end_NA_string(const char*);
  int countfields();
  bool skip_eol();
};

typedef void (*ParserFnPtr)(FreadTokenizer& ctx);





// Per-column per-thread temporary string buffers used to assemble processed
// string data. Length = `nstrcols`. Each element in this array has the
// following fields:
//     .buf -- memory region where all string data is stored.
//     .size -- allocation size of this memory buffer.
//     .ptr -- the `postprocessBuffer` stores here the total amount of string
//         data currently held in the buffer; while the `orderBuffer` function
//         puts here the offset within the global string buffer where the
//         current buffer should be copied to.
//     .idx8 -- index of the current column within the `buff8` array.
//     .idxdt -- index of the current column within the output DataTable.
//     .numuses -- synchronization lock. The purpose of this variable is to
//         prevent race conditions between threads that do memcpy, and another
//         thread that needs to realloc the underlying buffer. Without the lock,
//         if one thread is performing a mem-copy and the other thread wants to
//         reallocs the buffer, then the first thread will segfault in the
//         middle of its operation. In order to prevent this, we use this
//         `.numuses` variable: when positive it shows the number of threads
//         that are currently writing to the same buffer. However when this
//         variable is negative, it means the buffer is being realloced, and no
//         other threads is allowed to initiate a memcopy.
//
struct StrBuf {
  MemoryBuffer* mbuf;
  size_t idx8;
  size_t idxdt;
  size_t ptr;
  size_t sz;

  StrBuf(size_t allocsize, size_t i8, size_t idt) {
    mbuf = new MemoryMemBuf(allocsize);
    ptr = 0;
    idx8 = i8;
    idxdt = idt;
  }

  StrBuf(StrBuf&& other) {
    mbuf = other.mbuf;
    ptr = other.ptr;
    idx8 = other.idx8;
    idxdt = other.idxdt;
    other.mbuf = nullptr;
  }

  StrBuf(const StrBuf&) = delete;

  ~StrBuf() {
    if (mbuf) {
      mbuf->release();
      mbuf = nullptr;
    }
  }
};



#endif
