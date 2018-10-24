//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include "str/py_str.h"
#include <unordered_map>
#include <vector>
#include "datatable.h"
#include "utils/exceptions.h"
#include "utils/parallel.h"
#include "utils/shared_mutex.h"

namespace dt {

/**
 * Split string into tokens
 */
static void tokenize_string(
    std::vector<std::string>& tokens,
    const char* strstart,
    const char* strend,
    char sep
) {
  tokens.clear();
  const char* ch = strstart;
  while (ch < strend) {
    if (*ch == ' ' || *ch == '\t' || *ch == '\n' || *ch == sep) {
      ch++;
    }
    else {
      const char* ch0 = ch;
      // Handle the case of quoted token
      if (*ch == '\'' || *ch == '"') {
        char quote = *ch++;
        while (ch < strend && *ch != quote) {
          ch += 1 + (*ch == '\\');
        }
        if (ch < strend) {
          tokens.emplace_back(ch0 + 1, ch);
          ch++;  // move over the closing quote
          continue;
        }
        // fall-through
        ch = ch0;
      }
      // Regular non-quoted token, parse until the next sep
      while (ch < strend && *ch != sep) {
        ch++;
      }
      const char* ch1 = ch;
      while (ch1 > ch0) {  // also omit trailing spaces
        char c = *(ch1 - 1);
        if (!(c == ' ' || c == '\t' || c == '\n')) break;
        ch1--;
      }
      tokens.emplace_back(ch0, ch1);
      ch++;  // move over the sep
    }
  }
}



DataTable* split_into_nhot(Column* col, char sep) {
  bool is32 = (col->stype() == SType::STR32);
  xassert(is32 || (col->stype() == SType::STR64));
  const uint32_t* offsets32 = nullptr;
  const uint64_t* offsets64 = nullptr;
  const char* strdata;
  if (is32) {
    auto scol = static_cast<StringColumn<uint32_t>*>(col);
    offsets32 = scol->offsets();
    strdata = scol->strdata();
  } else {
    auto scol = static_cast<StringColumn<uint64_t>*>(col);
    offsets64 = scol->offsets();
    strdata = scol->strdata();
  }

  size_t nrows = static_cast<size_t>(col->nrows);
  std::unordered_map<std::string, size_t> colsmap;
  std::vector<Column*> outcols;
  std::vector<int8_t*> outdata;
  std::vector<std::string> outnames;
  dt::shared_mutex shmutex;

  OmpExceptionManager oem;
  #pragma omp parallel
  {
    try {
      size_t ith = static_cast<size_t>(omp_get_thread_num());
      size_t nth = static_cast<size_t>(omp_get_num_threads());
      std::vector<std::string> chunks;

      for (size_t irow = ith; irow < nrows; irow += nth) {
        const char* strstart, *strend;
        if (is32) {
          if (ISNA(offsets32[irow])) continue;
          strstart = strdata + (offsets32[irow - 1] & ~GETNA<uint32_t>());
          strend = strdata + offsets32[irow];
        } else {
          if (ISNA(offsets64[irow])) continue;
          strstart = strdata + (offsets64[irow - 1] & ~GETNA<uint64_t>());
          strend = strdata + offsets64[irow];
        }
        if (strstart == strend) continue;
        char chfirst = *strstart;
        char chlast = strend[-1];
        if ((chfirst == '(' && chlast == ')') ||
            (chfirst == '[' && chlast == ']') ||
            (chfirst == '{' && chlast == '}')) {
          strstart++;
          strend--;
        }

        tokenize_string(chunks, strstart, strend, sep);

        dt::shared_lock<dt::shared_mutex> lock(shmutex);
        for (const std::string& s : chunks) {
          if (colsmap.count(s)) {
            size_t j = colsmap[s];
            outdata[j][irow] = 1;
          } else {
            lock.exclusive_start();
            if (colsmap.count(s) == 0) {
              colsmap[s] = outcols.size();
              BoolColumn* newcol = new BoolColumn(col->nrows);
              int8_t* data = newcol->elements_w();
              std::memset(data, 0, nrows);
              data[irow] = 1;
              outcols.push_back(newcol);
              outdata.push_back(data);
              outnames.push_back(s);
            } else {
              // In case the name was already added from another thread while we
              // were waiting for the exclusive lock
              size_t j = colsmap[s];
              outdata[j][irow] = 1;
            }
            lock.exclusive_end();
          }
        }
      }
    } catch (...) {
      oem.capture_exception();  // LCOV_EXCL_LINE
    }                           // LCOV_EXCL_LINE
  }  // end of #pragma omp parallel

  oem.rethrow_exception_if_any();

  return new DataTable(std::move(outcols), std::move(outnames));
}


} // namespace dt
