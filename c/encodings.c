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
    const unsigned char *restrict src, int len,
    unsigned char *restrict dest, uint32_t *map
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
            memcpy(d, &m, s);
            d += s;
        }
    }
    return (int)(d - dest);
}


/**
 * Check whether the memory buffer contains a valid UTF-8 string.
 */
int is_valid_utf8(const unsigned char *restrict src, size_t len)
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
