//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <cstring>
#include <string.h>
#include "read/constants.h"
#include "encodings.h"



/**
 * Decode a string from the provided single-byte character set. On success
 * writes the UTF-8-encoded string to `dest` and returns the length of the new
 * string in bytes. On failure returns -1 - length of the string that was
 * succesfully encoded until the error occurred.
 *
 * @param src
 *     Memory buffer containing byte-string in a SBCS code page given by `map`.
 *
 * @param len
 *     Length of the byte-string `src`, in bytes.
 *
 * @param dest
 *     Memory buffer where the decoded UTF-8 string will be written. This buffer
 *     should be preallocated to allow writing of at least `len * 3` bytes
 *     (worst-case scenario).
 *
 * @param map
 *     Mapping of all 256 byte values in this SBCS into UTF-8 bytes represented
 *     as `uint32_t` constants. The following assumption about this mapping are
 *     made: values 0-127 are mapped to themselves; if some value in the range
 *     128-255 maps to 0 it means this byte is invalid in this code page; all
 *     values in the range 128-255 map to either 2- or 3-byte-long UTF-8 byte
 *     sequences.
 */
int decode_sbcs(
    const uint8_t* src, int len, uint8_t* dest, uint32_t *map
) {
  const unsigned char* end = src + len;
  unsigned char* d = dest;
  for (; src < end; src++) {
    unsigned char ch = *src;
    if (ch < 0x80) {
      *d++ = ch;
    } else {
      uint32_t m = map[ch];
      if (m == 0) return -static_cast<int>(1 + d - dest);
      int s = 2 + ((m & 0xFF0000) != 0);
      std::memcpy(d, &m, static_cast<size_t>(s));
      d += s;
    }
  }
  return static_cast<int>(d - dest);
}


/**
 * Check whether the memory buffer contains a valid UTF-8 string.
 */
int is_valid_utf8(const uint8_t* src, size_t len)
{
  const uint8_t* ch = src;
  const uint8_t* end = src + len;
  while (ch < end) {
    uint8_t c = *ch;
    if (c < 0x80) {
      ch++;
    } else {
      uint8_t cc = ch[1];
      if ((c & 0xE0) == 0xC0) {
        // 110xxxxx 10xxxxxx
        if ((cc & 0xC0) != 0x80 || (c & 0xFE) == 0xC0) {
          return 0;
        }
        ch += 2;
      } else if ((c & 0xF0) == 0xE0) {
        // 1110xxxx 10xxxxxx 10xxxxxx
        if ((cc & 0xC0) != 0x80 ||
            (ch[2] & 0xC0) != 0x80 ||
            (c == 0xE0 && (cc & 0xE0) == 0x80) ||  // overlong
            (c == 0xED && (cc & 0xE0) == 0xA0) ||  // surrogate
            (c == 0xEF && cc == 0xBF && (ch[2] & 0xFE) == 0xBE)) {
          return 0;
        }
        ch += 3;
      } else if ((c & 0xF8) == 0xF0) {
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((cc & 0xC0) != 0x80 ||
            (ch[2] & 0xC0) != 0x80 ||
            (ch[3] & 0xC0) != 0x80 ||
            (c == 0xF0 && (cc & 0xF0) == 0x80) ||  // overlong
            (c == 0xF4 && cc > 0x8F) ||  // unmapped
            c > 0xF4) {
          return 0;
        }
        ch += 4;
      } else
        return 0;
    }
  }
  return (ch == end);
}


/**
 * Check whether the string `src` contains valid UTF-8, or if there are any
 * escape characters `ech` inside it. The `ech` parameter should be either '\\'
 * or the quote char '"' / '\'' (any `ech` not in the ASCII range will be
 * ignored).
 *
 * @return 0 if this is a valid UTF-8 string with no escape chars; 1 if the
 *     string is valid UTF-8 but contains escape chars; and 2 if the string is
 *     not valid UTF-8.
 */
int check_escaped_string(const uint8_t* src, size_t len, uint8_t ech)
{
  const uint8_t* ch = src;
  const uint8_t* end = src + len;
  int has_escapes = 0;
  while (ch < end) {
    uint8_t c = *ch;
    if (c < 0x80) {
      has_escapes |= (c == ech);
      ch++;
    } else {
      uint8_t cc = ch[1];
      if ((c & 0xE0) == 0xC0) {
        // 110xxxxx 10xxxxxx
        if ((cc & 0xC0) != 0x80 || (c & 0xFE) == 0xC0) {
          return 2;
        }
        ch += 2;
      } else if ((c & 0xF0) == 0xE0) {
        // 1110xxxx 10xxxxxx 10xxxxxx
        if ((cc & 0xC0) != 0x80 ||
            (ch[2] & 0xC0) != 0x80 ||
            (c == 0xE0 && (cc & 0xE0) == 0x80) ||  // overlong
            (c == 0xED && (cc & 0xE0) == 0xA0) ||  // surrogate
            (c == 0xEF && cc == 0xBF && (ch[2] & 0xFE) == 0xBE)) {
          return 2;
        }
        ch += 3;
      } else if ((c & 0xF8) == 0xF0) {
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((cc & 0xC0) != 0x80 ||
            (ch[2] & 0xC0) != 0x80 ||
            (ch[3] & 0xC0) != 0x80 ||
            (c == 0xF0 && (cc & 0xF0) == 0x80) ||  // overlong
            (c == 0xF4 && cc > 0x8F) ||  // unmapped
            c > 0xF4) {
          return 2;
        }
        ch += 4;
      } else
        return 2;
    }
  }
  return (ch == end)? has_escapes : 2;
}



/**
 * Convert UTF-32 encoded buffer `buf` into UTF-8 and write the bytes into
 * buffer `ch`. The function will stop encoding upon encountering \0 byte,
 * or after processing `maxchars` characters, whichever happens first. The
 * function returns the number of bytes written to the output buffer. It is the
 * responsibility of the user to ensure that `ch` has sufficient space to write
 * all the bytes (no more than `maxchars*4` bytes will be needed).
 */
int64_t utf32_to_utf8(uint32_t* buf, size_t maxchars, char* ch)
{
  char* out0 = ch;
  uint32_t* end = buf + maxchars;
  while (buf < end) {
    uint32_t code = *buf++;
    if (!code) break;
    if (code <= 0x7F) {
      *ch++ = static_cast<char>(code);
    } else if (code <= 0x7FF) {
      *ch++ = static_cast<char>(0xC0 | (code >> 6));
      *ch++ = static_cast<char>(0x80 | (code & 0x3F));
    } else if (code <= 0xFFFF) {
      *ch++ = static_cast<char>(0xE0 | (code >> 12));
      *ch++ = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
      *ch++ = static_cast<char>(0x80 | (code & 0x3F));
    } else {
      *ch++ = static_cast<char>(0xF0 | (code >> 18));
      *ch++ = static_cast<char>(0x80 | ((code >> 12) & 0x3F));
      *ch++ = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
      *ch++ = static_cast<char>(0x80 | (code & 0x3F));
    }
  }
  return static_cast<int64_t>(ch - out0);
}


static void write_utf8_codepoint(int32_t cp, uint8_t** dest) {
  uint8_t* ch = *dest;
  if (cp <= 0x7F) {
    *ch++ = static_cast<uint8_t>(cp);
  } else if (cp <= 0x7FF) {
    *ch++ = 0xC0 | static_cast<uint8_t>(cp >> 6);
    *ch++ = 0x80 | static_cast<uint8_t>(cp & 0x3F);
  } else if (cp <= 0xFFFF) {
    *ch++ = 0xE0 | static_cast<uint8_t>(cp >> 12);
    *ch++ = 0x80 | static_cast<uint8_t>((cp >> 6) & 0x3F);
    *ch++ = 0x80 | static_cast<uint8_t>(cp & 0x3F);
  } else {
    *ch++ = 0xF0 | static_cast<uint8_t>(cp >> 18);
    *ch++ = 0x80 | static_cast<uint8_t>((cp >> 12) & 0x3F);
    *ch++ = 0x80 | static_cast<uint8_t>((cp >> 6) & 0x3F);
    *ch++ = 0x80 | static_cast<uint8_t>(cp & 0x3F);
  }
  *dest = ch;
}


int read_codepoint_from_utf8(const uint8_t** src) {
  int cp;
  auto ch = *src;
  auto c0 = *ch++;
  auto c1 = (*ch++) - 0x80;
  if ((c0 & 0xE0) == 0xC0) {
    cp = ((c0 - 0xC0) << 6) + c1;
  }
  else if ((c0 & 0xF0) == 0xE0) {
    auto c2 = (*ch++) - 0x80;
    cp = ((c0 - 0xE0) << 12) + (c1 << 6) + c2;
  }
  else {
    auto c2 = (*ch++) - 0x80;
    auto c3 = (*ch++) - 0x80;
    cp = ((c0 - 0xF0) << 18) + (c1 << 12) + (c2 << 6) + c3;
  }
  *src = ch;
  return cp;
}

/**
 * Decode a csv-encoded string. This function supports 2 encodings: either the
 * "doubled quotes" encoding, or "escape" encoding.
 * The first method is more commonly used in CSV: all quote characters within a
 * quoted string are doubled. For example, a string `he says "hello"!` becomes
 * `"he says ""hello""!"`. At the same time, the "escape" encoding converts it
 * into `"he says \"helo\"!"`.
 *
 * This method supports decoding both encodings, as determined by the `quote`
 * parameter. If any invalid sequences are encountered, an attempt is made to
 * fix them. No exceptions are thrown.
 *
 * In these methods `dest` will always be no longer than `src` in length, and
 * in fact the pointers `src` and `dest` may coincide to decode a string
 * in-place.
 *
 * Parameters
 * ----------
 * src, len
 *   The source string, given as a char pointer and a length. The string may
 *   contain embedded NUL characters.
 *
 * dest
 *   The destination buffer, where the decoded string will be written. This
 *   buffer should be at least as large as `len`.
 *
 * quote
 *   The quote character that should be un-doubled, or '\\' character for the
 *   "escaped" encoding.
 *
 * Returns
 * -------
 * The length of the decoded string.
 */
int decode_escaped_csv_string(
    const uint8_t* src, int len, uint8_t* dest, uint8_t quote
) {
  const uint8_t* ch = src;
  const uint8_t* end = src + len;
  uint8_t* d = dest;
  if (quote == '\\') {
    while (ch < end) {
      uint8_t c = *ch;
      if (c == '\\' && ch + 1 < end) {
        c = ch[1];
        ch += 2;
        switch (c) {
          case 'a': *d++ = '\a'; break;
          case 'b': *d++ = '\b'; break;
          case 'f': *d++ = '\f'; break;
          case 'n': *d++ = '\n'; break;
          case 'r': *d++ = '\r'; break;
          case 't': *d++ = '\t'; break;
          case 'v': *d++ = '\v'; break;
          case '0': case '1': case '2': case '3':
          case '4': case '5': case '6': case '7': {
            // Octal escape sequence
            uint8_t chd = c - '0';
            int32_t v = static_cast<int32_t>(chd);
            if (ch < end && (chd = *ch-'0') <= 7) {
              v = v * 8 + chd;
              ch++;
            }
            if (ch < end && (chd = *ch-'0') <= 7) {
              v = v * 8 + chd;
              ch++;
            }
            write_utf8_codepoint(v, &d);
            break;
          }
          case 'x': case 'u': case 'U': {
            // Hex-sequence
            int32_t v = 0;
            int n = (c == 'x')? 2 : (c == 'u')? 4 : 8;
            for (int i = 0; i < n; i++) {
              if (ch >= end) break;
              uint8_t chd = dt::read::hexdigits[*ch];
              if (chd == 99) break;
              v = v * 16 + chd;
              ch++;
            }
            write_utf8_codepoint(v, &d);
            break;
          }
          case '\\': case '"': case '\'': case '?':
          default:
            *d++ = c; break;
        }
      } else {
        *d = c;
        d++;
        ch++;
      }
    }
  } else {
    while (ch < end) {
      uint8_t c = *ch;
      if (c == quote) {
        ch += (ch + 1 < end && ch[1] == quote);
      }
      *d = c;
      ch++;
      d++;
    }
  }
  return static_cast<int>(d - dest);
}

