#include "utils/assert.h"
#include "py_encodings.h"
#include "py_utils.h"


static uint32_t win1252_map[256];
static uint32_t win1251_map[256];
static uint32_t iso8859_map[256];

static void initialize_map(uint32_t *map, int n, const char *encoding)
{
  for (int i = 0; i < n; i++) {
    PyObject *string = PyUnicode_Decode((const char*)&i, 4, encoding, "replace");
    PyObject *bytes = PyUnicode_AsEncodedString(string, "utf-8", "replace");
    char *raw = PyBytes_AsString(bytes);  // borrowed ref
    memcpy(map + i, raw, 4);
    Py_DECREF(string);
    Py_DECREF(bytes);
  }
}

int decode_iso8859(const unsigned char *__restrict__ src, int len,
                   unsigned char *__restrict__ dest)
{
    return decode_sbcs(src, len, dest, iso8859_map);
}

int decode_win1252(const unsigned char *__restrict__ src, int len,
                   unsigned char *__restrict__ dest)
{
    return decode_sbcs(src, len, dest, win1252_map);
}

int decode_win1251(const unsigned char *__restrict__ src, int len,
                   unsigned char *__restrict__ dest)
{
    return decode_sbcs(src, len, dest, win1251_map);
}



// Module initialization
int init_py_encodings(PyObject*)
{
  initialize_map(win1252_map, 256, "Windows-1252");
  initialize_map(win1251_map, 256, "Windows-1251");
  initialize_map(iso8859_map, 256, "ISO8859");

  // Sanity checks
  for (int k = 0; k < 3; k++) {
    uint32_t* map = (k == 0)? win1252_map : (k == 1)? win1251_map : iso8859_map;
    for (unsigned int i = 0; i < 256; i++) {
      if (i < 0x80) assert(map[i] == i);  // check ASCII compatibility
      if (i && map[i] == 0) map[i] = 0x00BDBFEF;  // U+FFFD
      assert((map[i] & 0xFF000000) == 0);
    }
  }
  return 1;
}
