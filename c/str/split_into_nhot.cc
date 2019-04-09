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
#include <string>         // std::string
#include <unordered_map>  // std::unordered_map
#include <vector>         // std::vector
#include "models/utils.h"        // sort_index
#include "parallel/api.h"
#include "parallel/shared_mutex.h"
#include "utils/exceptions.h"
#include "str/py_str.h"
#include "datatable.h"
#include "options.h"

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



DataTable* split_into_nhot(Column* col, char sep, bool sort /* = false */) {
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

  size_t nrows = col->nrows;
  std::unordered_map<std::string, size_t> colsmap;
  std::vector<Column*> outcols;
  std::vector<int8_t*> outdata;
  std::vector<std::string> outnames;
  dt::shared_mutex shmutex;
  const RowIndex& ri = col->rowindex();

  dt::parallel_region(
    /* nthreads = */ nrows,
    [&] {
      std::vector<std::string> chunks;

      dt::parallel_for_static(nrows,
        [&](size_t irow) {
          const char* strstart, *strend;
          size_t jrow = ri[irow];
          if (jrow == RowIndex::NA) return;
          if (is32) {
            if (ISNA(offsets32[jrow])) return;
            strstart = strdata + (offsets32[jrow - 1] & ~GETNA<uint32_t>());
            strend = strdata + offsets32[jrow];
          } else {
            if (ISNA(offsets64[jrow])) return;
            strstart = strdata + (offsets64[jrow - 1] & ~GETNA<uint64_t>());
            strend = strdata + offsets64[jrow];
          }
          if (strstart == strend) return;
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
        });
    });  // dt::parallel_region()

  // Re-order columns, so that column names go in alphabetical order.
  if (sort) {
    size_t ncols = outcols.size();
    std::vector<std::string> outnames_sorted(ncols);
    std::vector<Column*> outcols_sorted(ncols);
    std::vector<size_t> colindex = sort_index<std::string>(outnames);
    for (size_t i = 0; i < ncols; ++i) {
      size_t j = colindex[i];
      outnames_sorted[i] = std::move(outnames[j]);
      outcols_sorted[i] = outcols[j];
    }
    outcols = std::move(outcols_sorted);
    outnames = std::move(outnames_sorted);
  }

  return new DataTable(std::move(outcols), std::move(outnames));
}


} // namespace dt
