//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include <cstring>
#include "py_encodings.h"
#include "utils/assert.h"


static uint32_t win1252_map[256];
static uint32_t win1251_map[256];
static uint32_t iso8859_map[256];

static void initialize_map(uint32_t* map, int n, const char* encoding)
{
  for (int32_t i = 0; i < n; i++) {
    const char* i_as_str = reinterpret_cast<const char*>(&i);
    PyObject* string = PyUnicode_Decode(i_as_str, 4, encoding, "replace");
    PyObject* bytes = PyUnicode_AsEncodedString(string, "utf-8", "replace");
    char *raw = PyBytes_AsString(bytes);  // borrowed ref
    std::memcpy(map + i, raw, 4);
    Py_DECREF(string);
    Py_DECREF(bytes);
  }
}

int decode_iso8859(const uint8_t* src, int len, uint8_t* dest) {
  return decode_sbcs(src, len, dest, iso8859_map);
}

int decode_win1252(const uint8_t* src, int len, uint8_t* dest) {
  return decode_sbcs(src, len, dest, win1252_map);
}

int decode_win1252(const char* src, int len, char* dest) {
  return decode_sbcs(reinterpret_cast<const uint8_t*>(src), len,
                     reinterpret_cast<uint8_t*>(dest), win1252_map);
}

int decode_win1251(const uint8_t* src, int len, uint8_t* dest) {
  return decode_sbcs(src, len, dest, win1251_map);
}



// Module initialization
int init_py_encodings(PyObject*) {
  initialize_map(win1252_map, 256, "Windows-1252");
  initialize_map(win1251_map, 256, "Windows-1251");
  initialize_map(iso8859_map, 256, "ISO8859");

  // Sanity checks
  for (int k = 0; k < 3; k++) {
    uint32_t* map = (k == 0)? win1252_map : (k == 1)? win1251_map : iso8859_map;
    for (unsigned int i = 0; i < 256; i++) {
      if (i < 0x80) {
        xassert(map[i] == i);  // check ASCII compatibility
      }

      if (i && map[i] == 0) map[i] = 0x00BDBFEF;  // U+FFFD
      xassert((map[i] & 0xFF000000) == 0);
    }
  }
  return 1;
}
