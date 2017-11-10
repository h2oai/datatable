//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "csv/reader.h"


// Forward-declare static helper functions
static void skip_whitespace(const char** pch);
static void skip_ext_whitespace(const char** pch);
static bool read_keyword(const char** pch, const char* keyword);
static bool read_name(const char** pch, const char** start, size_t* length);


//------------------------------------------------------------------------------

ArffReader::ArffReader(GenericReader& gr) : greader(gr)
{
  verbose = gr.get_verbose();
}

ArffReader::~ArffReader() {}


std::unique_ptr<DataTable> ArffReader::read() {
  if (verbose) printf("[ARFF reader]\n");
  const char* ch = greader.dataptr();

  read_preamble(&ch);
  if (verbose) {
    size_t sz = preamble.length();
    if (sz)
      printf("  Preamble found: %zu bytes\n", sz);
    else
      printf("  Preamble not found\n");
  }

  bool res = read_relation(&ch);
  if (res) {
    if (verbose) printf("  @relation token found: name = '%s'\n", name.c_str());
  } else {
    if (verbose) printf("  @relation not found: this is not an ARFF file\n");
    return nullptr;
  }
  return nullptr;
}



//------------------------------------------------------------------------------

/**
 * This is similar to `skip_ext_whitespace`, except the text of the comments is
 * saved into `preamble`.
 */
void ArffReader::read_preamble(const char** pch) {
  const char* ch = *pch;
  MemoryWritableBuffer out(64);

  while (true) {
    skip_whitespace(&ch);
    if (*ch == '%') {
      ch++;
      const char* ch0 = ch;
      while (*ch && *ch != '\n' && *ch != '\r') ch++;
      while (*ch == '\n' || *ch == '\r') ch++;
      size_t len = static_cast<size_t>(ch - ch0);
      out.write(len, ch0);
    } else if (*ch == '\n' || *ch == '\r') {
      ch++;
    } else {
      break;
    }
  }
  *pch = ch;
  out.finalize();
  preamble = out.get_string();
}


/**
 * From the documentation:
 *
 * | The relation name is defined as the first line in the ARFF file. The
 * | format is:
 * |
 * |    \@relation <relation-name>
 * |
 * | where <relation-name> is a string. The string must be quoted if the name
 * | includes spaces. Furthermore, relation names or attribute names (see
 * | below) cannot begin with (a) a character below U+0021, (b) '{', '}', ',',
 * | or '%'. Moreover, it can only begin with a single or double quote if there
 * | is a corresponding quote at the end of the name.
 * |
 * | ... The \@RELATION declaration is case-insensitive.
 *
 */
bool ArffReader::read_relation(const char** pch) {
  skip_ext_whitespace(pch);
  if (!read_keyword(pch, "@relation")) return false;
  skip_whitespace(pch);
  const char* nameStart;
  size_t nameLen;
  if (!read_name(pch, &nameStart, &nameLen)) return false;
  name = std::string(nameStart, nameLen);
  skip_whitespace(pch);
  return true;
}


/**
 * If `keyword` is present at the current location in the input, returns true
 * and advances pointer `pch`; otherwise returns false and leaves pointer `pch`
 * unmodified. The keyword is matched case-insensitively. Both the keyword and
 * the input are assumed to be \0-terminated. The keyword must also be followed
 * by whitespace (or newline) in  order to match.
 */
static bool read_keyword(const char** pch, const char* keyword) {
  const char* ch = *pch;
  while (true) {
    char ck = *keyword;
    char cc = *ch;
    if (!ck) {
      if (!cc || cc==' ' || cc=='\t' || cc=='\n' || cc=='\r') {
        *pch = ch;
        return true;
      }
      return false;
    }
    uint8_t ccl = static_cast<uint8_t>(cc - 'a');
    uint8_t ccu = static_cast<uint8_t>(cc - 'A');
    uint8_t ckl = static_cast<uint8_t>(ck - 'a');
    uint8_t cku = static_cast<uint8_t>(ck - 'A');
    if (!((ck == cc) ||
          (ckl < 26 && ckl == ccu) ||
          (cku < 26 && cku == ccl))) return false;
    ch++;
    keyword++;
  }
}


/**
 * Advances the pointer `pch` to the next non-whitespace character on the
 * current line. Only spaces and tabs are considered whitespace.
 */
static void skip_whitespace(const char** pch) {
  const char* ch = *pch;
  while (*ch == ' ' || *ch == '\t') ch++;
  *pch = ch;
}


/**
 * Skips possibly multi-line whitespace, i.e. same as `skip_whitespace`, but
 * also skips over newlines and comments.
 */
static void skip_ext_whitespace(const char** pch) {
  const char* ch = *pch;
  while (true) {
    if (*ch == ' ' || *ch == '\t' || *ch == '\r' || *ch == '\n') {
      ch++;
    } else if (*ch == '%') {
      while (*ch && *ch != '\n' && *ch != '\r') ch++;
    } else {
      *pch = ch;
      return;
    }
  }
}

static bool read_name(const char** pch, const char** start, size_t* len) {
  const char* ch0 = *pch;
  if (*ch0 == '"' || *ch0 == '\'') {
    char quote = *ch0;
    ch0++;
    const char* ch1 = ch0;
    while (*ch1 && *ch1 != quote && *ch1 != '\n' && *ch1 != '\r') ch1++;
    if (*ch1 == quote) {
      *start = ch0;
      *len = static_cast<size_t>(ch1 - ch0);
      *pch = ch1 + 1;
      return true;
    }
  } else if (static_cast<uint8_t>(*ch0) > 0x20 &&
             *ch0 != ',' && *ch0 != '{' && *ch0 != '}' && *ch0 != '%') {
    const char* ch1 = ch0;
    while (*ch1 && *ch1 != ' ' && *ch1 != '\t' &&
           *ch1 != '\r' && *ch1 != '\n') ch1++;
    *start = ch0;
    *len = static_cast<size_t>(ch1 - ch0);
    *pch = ch1;
    return true;
  }
  return false;
}
