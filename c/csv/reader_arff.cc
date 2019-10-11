//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/reader_arff.h"
#include "utils/exceptions.h"

// Forward-declare static helper functions
static bool read_name(const char** pch, const char** start, size_t* length);



//------------------------------------------------------------------------------

ArffReader::ArffReader(GenericReader& greader) : g(greader) {}

ArffReader::~ArffReader() {}

std::unique_ptr<DataTable> ArffReader::read_all() {
  g.trace("[ARFF reader]");
  ch = g.dataptr();
  line = 1;

  read_preamble();
  read_relation();
  if (name.empty()) return nullptr;
  read_attributes();
  read_data_decl();

  return nullptr;
}



//------------------------------------------------------------------------------

void ArffReader::read_preamble() {
  MemoryWritableBuffer out(256);  // initial allocation size is arbitrary
  while (true) {
    read_whitespace();
    if (*ch == '%') {
      ch++;  // step over '%'
      const char* ch0 = ch;
      while (*ch && *ch != '\n' && *ch != '\r') ch++;  // skip until eol
      skip_newlines();
      size_t len = static_cast<size_t>(ch - ch0);
      out.write(len, ch0);
    } else if (*ch == '\n' || *ch == '\r') {
      skip_newlines();
    } else {
      break;
    }
  }
  out.finalize();
  preamble = out.get_string();
  if (verbose && !preamble.empty()) {
    printf("  Preamble found (%zu bytes), file info begins on line %d\n",
           preamble.length(), line);
  }
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
void ArffReader::read_relation() {
  const char* nameStart = nullptr;
  size_t nameLen = 0;
  bool res = read_keyword("@relation") &&
             read_whitespace() &&
             read_name(&ch, &nameStart, &nameLen) &&
             read_end_of_line();
  if (res && nameLen) {
    name = std::string(nameStart, nameLen);
    if (verbose) printf("  @relation name = '%s'\n", name.c_str());
  } else {
    if (verbose) printf("  @relation declaration not found: this is not an ARFF "
                        "file\n");
  }
}


void ArffReader::read_attributes() {
  const char* start;
  size_t len;
  while (true) {
    if (!(read_keyword("@attribute") &&
          read_whitespace())) break;
    bool res = read_name(&ch, &start, &len);
    if (!res) {
      throw IOError() <<
          "Invalid @attribute in line " << line << " of the ARFF file: the "
          "name is missing";
    }
    std::string attrName = std::string(start, len);
    read_whitespace();
    // ColumnSpec::Type coltype = ColumnSpec::Type::Drop;
    if (*ch == '{') {
      ch++;
      read_whitespace();
      for (int n = 0; ; ++n) {
        res = read_name(&ch, &start, &len);
        if (!res) {
          throw IOError() <<
              "Invalid categorical @attribute '" << attrName << "' in line " <<
              line << " of the ARFF file: level " << (n+1) << " is ill-formed";
        }
        read_whitespace();
        int comma = (*ch == ',');
        ch += comma;
        read_whitespace();
        if (*ch == '}') { ch++; break; }
        if (!comma) {
          throw IOError() <<
              "Invalid categorical @attribute '" << attrName << "' in line " <<
              line << " of the ARFF file: expected a closing brace '}'";
        }
      }
      // coltype = ColumnSpec::Type::String;
    } else if (read_keyword("numeric") || read_keyword("real")) {
      // coltype = ColumnSpec::Type::Real;
    } else if (read_keyword("integer")) {
      // coltype = ColumnSpec::Type::Integer;
    } else if (read_keyword("string")) {
      // coltype = ColumnSpec::Type::String;
    }
    // columns.push_back(ColumnSpec(attrName, coltype));
    skip_ext_whitespace();
  }
  if (columns.empty()) {
    throw IOError() << "Invalid ARFF file: @attribute declarations are missing";
  }
  if (verbose) {
    printf("  Detected %zu columns\n", columns.size());
  }
}


void ArffReader::read_data_decl() {
  bool res = read_keyword("@data") &&
             read_end_of_line();
  if (!res) {
    throw IOError() << "Invalid ARFF file: @data section is missing";
  }
  if (verbose) {
    printf("  Data begins on line %d\n", line);
  }
}



//------------------------------------------------------------------------------

// keyword should contain only lowercase letters
bool ArffReader::read_keyword(const char* keyword) {
  const char* ch0 = ch;
  while (*keyword) {
    if (((*keyword - *ch) & ~32) == 0) {
      ch++;
      keyword++;
    } else {
      ch = ch0;
      return false;
    }
  }
  return true;
}


bool ArffReader::read_whitespace() {
  const char* ch0 = ch;
  while (*ch == ' ' || *ch == '\t') ch++;
  return (ch > ch0);
}


bool ArffReader::read_end_of_line() {
  while (*ch == ' ' || *ch == '\t') ch++;
  char c = *ch;
  if (c == '\0' || c == '\r' || c == '\n' || c == '%') {
    skip_ext_whitespace();
    return true;
  }
  return false;
}

void ArffReader::skip_newlines() {
  while (true) {
    if (*ch == '\n') {
      ch += 1 + (ch[1] == '\r');
      line++;
    } else if (*ch == '\r') {
      ch += 1 + (ch[1] == '\n');
      line++;
    } else {
      return;
    }
  }
}


void ArffReader::skip_ext_whitespace() {
  while (true) {
    if (*ch == ' ' || *ch == '\t') {
      ch++;
    } else if (*ch == '\r' || *ch == '\n') {
      skip_newlines();
    } else if (*ch == '%') {
      while (*ch && *ch != '\n' && *ch != '\r') ch++;
    } else {
      return;
    }
  }
}


/**
 * Read a "name" from the current input location. The name can be either quoted
 * or a bareword. Bareword names cannot start with any of '%', ',', '{', '}', or
 * characters in the range U+0000 - U+0020, and cannot contain whitespace.
 * If successful, this function returns true, advances the pointer `pch`, and
 * saves the pointer to the beginning of the name in `start` and name's length
 * in `len`. If not successful, the function returns false.
 */
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
