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
#include <cstring>                 // std::memcpy
#include "csv/reader_fread.h"      // FreadReader
#include "read/fread/fread_thread_context.h"
#include "read/parallel_reader.h"  // ChunkCoordinates
#include "read/parsers/ptype_iterator.h"     // PTypeIterator
#include "utils/misc.h"            // wallclock
#include "encodings.h"             // check_escaped_string, decode_escaped_csv_string
#include "py_encodings.h"          // decode_win1252
namespace dt {
namespace read {



FreadThreadContext::FreadThreadContext(
    size_t bcols, size_t brows, FreadReader& f, PT* types
  ) : ThreadContext(bcols, brows, f.preframe),
      global_types_(types),
      freader(f),
      parsers(parser_functions)
{
  parse_ctx_ = f.makeTokenizer();
  parse_ctx_.target = tbuf.data();
  ttime_push = 0;
  ttime_read = 0;
  quote = f.quote;
  quoteRule = f.quoteRule;
  sep = f.sep;
  verbose = f.verbose;
  fill = f.fill;
  skipEmptyLines = f.skip_blank_lines;
  numbersMayBeNAs = f.number_is_na;
}

FreadThreadContext::~FreadThreadContext() {
  freader.fo.time_push_data += ttime_push;
  freader.fo.time_read_data += ttime_read;
  ttime_push = 0;
  ttime_read = 0;
}



void FreadThreadContext::read_chunk(
  const ChunkCoordinates& cc, ChunkCoordinates& actual_cc)
{
  double t0 = verbose? wallclock() : 0;
  actual_cc.set_start_exact(cc.get_start());
  actual_cc.set_end_exact(nullptr);

  size_t ncols = preframe_.ncols();
  bool fillme = fill || (ncols==1 && !skipEmptyLines);
  bool fastParsingAllowed = (sep != ' ') && !numbersMayBeNAs;
  const char*& tch = parse_ctx_.ch;
  tch = cc.get_start();
  used_nrows = 0;
  parse_ctx_.target = tbuf.data();
  parse_ctx_.bytes_written = 0;
  local_types_.clear();
  PT* types = global_types_;

  while (tch < cc.get_end()) {
    if (used_nrows == tbuf_nrows) {
      allocate_tbuf(tbuf_ncols, tbuf_nrows * 3 / 2);
      parse_ctx_.target = tbuf.data() + used_nrows * tbuf_ncols;
    }
    const char* tlineStart = tch;  // for error message
    const char* fieldStart = tch;
    size_t j = 0;

    //*** START HOT ***//
    if (fastParsingAllowed) {
      // Try most common and fastest branch first: no whitespace, no numeric NAs, blank means NA
      while (j < ncols) {
        fieldStart = tch;
        parsers[types[j]](parse_ctx_);
        if (tch >= parse_ctx_.eof || *tch != sep) break;
        parse_ctx_.target ++;
        tch++;
        j++;
      }

      const char* fieldEnd = tch;
      if (tch == tlineStart) {
        parse_ctx_.skip_whitespace_at_line_start();
        if (tch == parse_ctx_.eof) break;  // empty last line
        if (skipEmptyLines && parse_ctx_.skip_eol()) continue;
        tch = tlineStart;  // in case white space at the beginning may need to be included in field
      }
      else if (parse_ctx_.skip_eol() && j < ncols) {
        parse_ctx_.target ++;
        j++;
        if (j==ncols) { used_nrows++; continue; }  // next line
        // TODO: fill the rest of the fields with NAs explicitly, without
        //       going through type-bumping process below
        tch = fieldEnd;
      }
      else {
        tch = fieldStart;
      }
    }

    if (sep==' ') {
      while (tch < parse_ctx_.eof && *tch==' ') tch++;
      fieldStart = tch;
      if (skipEmptyLines && parse_ctx_.skip_eol()) continue;
    }

    if (fillme || (tch == parse_ctx_.eof || (*tch!='\n' && *tch!='\r'))) {  // also includes the case when sep==' '
      while (j < ncols) {
        fieldStart = tch;
        // auto ptype_iter = preframe_.column(j).get_ptype_iterator(&parse_ctx_.quoteRule);
        PTypeIterator ptype_iter(types[j], preframe_.column(j).get_rtype(),
                                 &parse_ctx_.quoteRule);

        while (true) {
          tch = fieldStart;
          bool quoted = false;
          if (!parser_infos[*ptype_iter].type().is_string()) {
            parse_ctx_.skip_whitespace();
            const char* afterSpace = tch;
            tch = parse_ctx_.end_NA_string(tch);
            parse_ctx_.skip_whitespace();
            if (!parse_ctx_.at_end_of_field()) tch = afterSpace;
            if (tch < parse_ctx_.eof && *tch==quote) { quoted=true; tch++; }
          }
          parsers[*ptype_iter](parse_ctx_);
          if (quoted) {
            if (tch < parse_ctx_.eof && *tch==quote) tch++;
            else goto typebump;
          }
          parse_ctx_.skip_whitespace();
          if (parse_ctx_.at_end_of_field()) {
            if (sep==' ' && tch < parse_ctx_.eof && *tch==' ') {
              while ((tch + 1 < parse_ctx_.eof) && tch[1]==' ') tch++;  // multiple space considered one sep so move to last
              if (((tch + 1 < parse_ctx_.eof) && (tch[1]=='\r' || tch[1]=='\n'))
                  || (tch + 1 == parse_ctx_.eof)) tch++;
            }
            break;
          }

          // Only perform bumping type / quote rules, when we are sure that the
          // start of the chunk is valid.
          // Otherwise, we are not able to read the chunk, and therefore return.
          typebump:
          if (cc.is_start_exact()) {
            ++ptype_iter;
            tch = fieldStart;
          } else {
            return;
          }
        }

        // Type-bump. This may only happen if cc.is_start_exact() is true, which
        // can only happen to one thread at a time. Thus, there is no need
        // for "critical" section here.
        auto& colj = preframe_.column(j);
        if (ptype_iter.has_incremented()) {
          xassert(cc.is_start_exact());
          if (verbose) {
            freader.fo.type_bump_info(j + 1, colj, *ptype_iter, fieldStart,
                                      tch - fieldStart,
                                      // TODO: use line number instead
                                      static_cast<int64_t>(row0_ + used_nrows + freader.line));
          }
          if (local_types_.empty()) {
            local_types_.resize(ncols);
            types = local_types_.data();
            std::memcpy(types, global_types_, sizeof(PT) * ncols);
          }
          types[j] = *ptype_iter;
        }
        parse_ctx_.target ++;
        j++;
        if (tch < parse_ctx_.eof && *tch==sep) { tch++; continue; }
        if (fill && (tch == parse_ctx_.eof || *tch=='\n' || *tch=='\r') && j <= ncols) {
          // All parsers have already stored NA to target; except for string
          // which writes "" value instead -- hence this case should be
          // corrected here.
          if (colj.is_string() && parse_ctx_.target[-1].str32.length == 0) {
            parse_ctx_.target[-1].str32.setna();
          }
          continue;
        }
        break;
      }  // while (j < ncols)
    }

    if (j < ncols) {
      // Is it perhaps an empty line at the end of the input? If so then it
      // should be simply skipped without raising any errors
      if (j <= 1) {
        tch = fieldStart;
        parse_ctx_.skip_whitespace_at_line_start();
        while (parse_ctx_.skip_eol()) {
          parse_ctx_.skip_whitespace();
        }
        if (parse_ctx_.ch == parse_ctx_.eof) break;
      }

      // not enough columns observed (including empty line). If fill==true,
      // fields should already have been filled above due to continue inside
      // `while (j < ncols)`.
      if (cc.is_start_exact()) {
        throw IOError() << "Too few fields on line "
          << row0_ + used_nrows + freader.line  // TODO: use line number instead
          << ": expected " << ncols << " but found only " << j
          << " (with sep='" << sep << "'). Set fill=True to ignore this error. "
          << " <<" << freader.repr_source(tlineStart, 500) << ">>";
      } else {
        return;
      }
    }

    if (!(parse_ctx_.skip_eol() || tch == parse_ctx_.eof)) {
      if (cc.is_start_exact()) {
        throw IOError() << "Too many fields on line "
          << row0_ + used_nrows + freader.line  // TODO: use line number instead
          << ": expected " << ncols << " but more are present. <<"
          << freader.repr_source(tlineStart, 500) << ">>";
      } else {
        return;
      }
    }
    used_nrows++;
  }

  if (local_types_.empty()) {
    preorder();
  }

  // Tell the caller where we finished reading the chunk. This is why
  // the parameter `actual_cc` was passed to this function.
  actual_cc.set_end_exact(tch);
  if (verbose) ttime_read += wallclock() - t0;
}



void FreadThreadContext::postorder() {
  double t0 = verbose? wallclock() : 0;
  ThreadContext::postorder();
  if (verbose) ttime_push += wallclock() - t0;
}


bool FreadThreadContext::handle_typebumps(OrderedTask* otask) {
  if (local_types_.empty()) return false;
  otask->super_ordered([&]{
    auto tempfile = preframe_.get_tempfile();
    size_t ncols = local_types_.size();
    for (size_t i = 0; i < ncols; ++i) {
      auto ptype = local_types_[i];
      if (ptype != global_types_[i]) {
        global_types_[i] = ptype;
        auto& inpcol = preframe_.column(i);
        inpcol.set_ptype(ptype);

        auto& outcol = preframe_.column(i).outcol();
        outcol.set_stype(inpcol.get_stype(), row0_, tempfile);
      }
    }
    local_types_.clear();
  });
  return true;
}





}}  // namespace dt::read
