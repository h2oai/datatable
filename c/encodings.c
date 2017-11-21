#include <string.h>
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
    const unsigned char *__restrict__ src, int len,
    unsigned char *__restrict__ dest, uint32_t *map
) {
    const unsigned char *end = src + len;
    unsigned char *d = dest;
    for (; src < end; src++) {
        unsigned char ch = *src;
        if (ch < 0x80) {
            *d++ = ch;
        } else {
            uint32_t m = map[ch];
            if (m == 0) return -(int)(1 + d - dest);
            int s = 2 + ((m & 0xFF0000) != 0);
            memcpy(d, &m, (size_t) s);
            d += s;
        }
    }
    return (int)(d - dest);
}


/**
 * Check whether the memory buffer contains a valid UTF-8 string.
 */
int is_valid_utf8(const unsigned char *__restrict__ src, size_t len)
{
    const unsigned char *ch = src;
    const unsigned char *end = src + len;
    while (ch < end) {
        unsigned char c = *ch;
        if (c < 0x80) {
            ch++;
        } else {
            unsigned char cc = ch[1];
            if ((c & 0xE0) == 0xC0) {
                // 110xxxxx 10xxxxxx
                if ((cc & 0xC0) != 0x80 ||
                    (c & 0xFE) == 0xC0)
                    return 0;
                ch += 2;
            } else if ((c & 0xF0) == 0xE0) {
                // 1110xxxx 10xxxxxx 10xxxxxx
                if ((cc & 0xC0) != 0x80 ||
                    (ch[2] & 0xC0) != 0x80 ||
                    (c == 0xE0 && (cc & 0xE0) == 0x80) ||  // overlong
                    (c == 0xED && (cc & 0xE0) == 0xA0) ||  // surrogate
                    (c == 0xEF && cc == 0xBF && (ch[2] & 0xFE) == 0xBE))
                    return 0;
                ch += 3;
            } else if ((c & 0xF8) == 0xF0) {
                // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                if ((cc & 0xC0) != 0x80 ||
                    (ch[2] & 0xC0) != 0x80 ||
                    (ch[3] & 0xC0) != 0x80 ||
                    (c == 0xF0 && (cc & 0xF0) == 0x80) ||  // overlong
                    (c == 0xF4 && cc > 0x8F) ||  // unmapped
                    c > 0xF4)
                    return 0;
                ch += 4;
            } else
                return 0;
        }
    }
    return (ch == end);
}


/**
 * Convert UTF-32 encoded buffer `buf` into UTF-8 and write the bytes into
 * buffer `ch`. The function will stop encoding upon encountering \0 byte,
 * or after processing `maxchars` characters, whichever happens first. The
 * function returns the number of bytes written to the output buffer. It is the
 * responsibility of the user to ensure that `ch` has sufficient space to write
 * all the bytes (no more than `maxchars*4` bytes will be needed).
 */
int64_t utf32_to_utf8(uint32_t* buf, int64_t maxchars, char* ch)
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
