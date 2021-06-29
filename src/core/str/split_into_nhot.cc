//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include <cstring>        // std::memset
#include <string>         // std::string
#include <unordered_map>  // std::unordered_map
#include <vector>         // std::vector
#include "datatable.h"
#include "documentation.h"
#include "frame/py_frame.h"
#include "ltype.h"
#include "models/utils.h" // sort_index
#include "options.h"
#include "parallel/api.h"
#include "parallel/shared_mutex.h"
#include "python/xargs.h"
#include "stype.h"
#include "utils/exceptions.h"
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


/**
 * Encode NA's with NA's
 */
static void encode_nones(const Column& col, colvec& outcols) {
  size_t ncols = outcols.size();
  if (ncols == 0) return;

  size_t nrows = outcols[0].nrows();
  std::vector<int8_t*> coldata(ncols);
  for (size_t i = 0; i < ncols; ++i) {
    coldata[i] = static_cast<int8_t*>(outcols[i].get_data_editable());
  }

  dt::parallel_for_dynamic(nrows,
    [&](size_t irow) {
      CString s;
      bool isvalid = col.get_element(irow, &s);
      if (!isvalid) {
        for (size_t i = 0; i < ncols; ++i) {
          coldata[i][irow] = GETNA<int8_t>();
        }
      }
    }
  );
}


/**
 * Re-order columns, so that column names go in alphabetical order.
 */
static void sort_colnames(colvec& outcols, strvec& outnames) {
  size_t ncols = outcols.size();
  strvec outnames_sorted(ncols);
  colvec outcols_sorted(ncols);
  sztvec colindex = sort_index<std::string>(outnames);

  for (size_t i = 0; i < ncols; ++i) {
    size_t j = colindex[i];
    outnames_sorted[i] = std::move(outnames[j]);
    outcols_sorted[i] = std::move(outcols[j]);
  }

  outcols = std::move(outcols_sorted);
  outnames = std::move(outnames_sorted);
}


static DataTable* split_into_nhot(const Column& col, char sep,
                                  bool sort /* = false */)
{
  xassert(col.ltype() == LType::STRING);

  size_t nrows = col.nrows();
  std::unordered_map<std::string, size_t> colsmap;
  strvec outnames;
  colvec outcols;
  std::vector<int8_t*> outdata;
  dt::shared_mutex shmutex;

  dt::parallel_region(
    NThreads(nrows),
    [&] {
      std::vector<std::string> chunks;

      dt::nested_for_static(nrows,
        [&](size_t irow) {
          CString str;
          bool isvalid = col.get_element(irow, &str);
          if (!isvalid || str.size() == 0) return;
          const char* strstart = str.data();
          const char* strend = str.end();

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
                auto newcol = Column::new_data_column(nrows, SType::BOOL);
                int8_t* data = static_cast<int8_t*>(newcol.get_data_editable());
                std::memset(data, 0, nrows);
                data[irow] = 1;
                outcols.push_back(std::move(newcol));
                outdata.push_back(data);
                outnames.push_back(s);
              } else {
                // In case the name was already added from another thread while
                // we were waiting for the exclusive lock
                size_t j = colsmap[s];
                outdata[j][irow] = 1;
              }
              lock.exclusive_end();
            }
          }
        });
    });  // dt::parallel_region()

  // At this point NA's are encoded with zeros, here we encode them with NA's.
  encode_nones(col, outcols);

  if (sort) sort_colnames(outcols, outnames);

  return new DataTable(std::move(outcols), std::move(outnames));
}



//------------------------------------------------------------------------------
// Python-facing functions
//------------------------------------------------------------------------------

static py::oobj py_split_into_nhot(const py::XArgs& args) {
  if (args[0].is_undefined()) {
    throw ValueError() << "Required parameter `frame` is missing";
  }

  if (args[0].is_none()) {
    return py::None();
  }

  DataTable* dt = args[0].to_datatable();
  std::string sep = args[1]? args[1].to_string() : ",";
  bool sort = args[2]? args[2].to_bool_strict() : false;

  if (dt->ncols() != 1) {
    throw ValueError() << "Function split_into_nhot() may only be applied to "
      "a single-column Frame of type string;" << " got frame with "
      << dt->ncols() << " columns";
  }
  const Column& col0 = dt->get_column(0);
  dt::SType st = col0.stype();
  if (!(st == dt::SType::STR32 || st == dt::SType::STR64)) {
    throw TypeError() << "Function split_into_nhot() may only be applied to "
      "a single-column Frame of type string;" << " received a column of type "
      << st;
  }
  if (sep.size() != 1) {
    throw ValueError() << "Parameter `sep` in split_into_nhot() must be a "
      "single character; got '" << sep << "'";
  }

  DataTable* res = split_into_nhot(col0, sep[0], sort);
  return py::Frame::oframe(res);
}

DECLARE_PYFN(&py_split_into_nhot)
    ->name("split_into_nhot")
    ->docs(dt::doc_str_split_into_nhot)
    ->n_positional_args(1)
    ->n_keyword_args(2)
    ->arg_names({"frame", "sep", "sort"});



static py::oobj py_split_into_nhot_deprecated(const py::XArgs& args) {
  auto w = DeprecationWarning();
  w << "Function `dt.split_into_nhot()` is deprecated since version 1.0.0, "
       "and will be removed in version 1.1.0.\n"
       "Please use `dt.str.split_into_nhot()` instead.";
  w.emit_warning();
  return py_split_into_nhot(args);
}

DECLARE_PYFN(&py_split_into_nhot_deprecated)
    ->name("split_into_nhot_depr")
    ->docs(dt::doc_dt_split_into_nhot)
    ->n_positional_args(1)
    ->n_keyword_args(2)
    ->arg_names({"frame", "sep", "sort"});




} // namespace dt
