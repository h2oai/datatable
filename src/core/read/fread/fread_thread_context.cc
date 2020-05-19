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
#include "csv/reader_fread.h"      // FreadReader
#include "read/fread/fread_thread_context.h"
#include "read/parallel_reader.h"  // ChunkCoordinates
#include "utils/misc.h"            // wallclock
#include "encodings.h"             // check_escaped_string, decode_escaped_csv_string
#include "py_encodings.h"          // decode_win1252
namespace dt {
namespace read {



FreadThreadContext::FreadThreadContext(
    size_t bcols, size_t brows, FreadReader& f, PT* types_
  ) : ThreadContext(bcols, brows, f.preframe),
      types(types_),
      freader(f),
      tokenizer(f.makeTokenizer()),
      parsers(ParserLibrary::get_parser_fns())
{
  tokenizer.target = tbuf.data();
  ttime_push = 0;
  ttime_read = 0;
  anchor = nullptr;
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
  const char*& tch = tokenizer.ch;
  tch = cc.get_start();
  used_nrows = 0;
  tokenizer.target = tbuf.data();
  tokenizer.anchor = anchor = tch;

  while (tch < cc.get_end()) {
    if (used_nrows == tbuf_nrows) {
      allocate_tbuf(tbuf_ncols, tbuf_nrows * 3 / 2);
      tokenizer.target = tbuf.data() + used_nrows * tbuf_ncols;
    }
    const char* tlineStart = tch;  // for error message
    const char* fieldStart = tch;
    size_t j = 0;

    //*** START HOT ***//
    if (fastParsingAllowed) {
      // Try most common and fastest branch first: no whitespace, no numeric NAs, blank means NA
      while (j < ncols) {
        fieldStart = tch;
        parsers[types[j]](tokenizer);
        if (tch >= tokenizer.eof || *tch != sep) break;
        tokenizer.target += preframe_.column(j).is_in_buffer();
        tch++;
        j++;
      }
      //*** END HOT. START TEPID ***//
      if (tch == tlineStart) {
        tokenizer.skip_whitespace_at_line_start();
        if (tch == tokenizer.eof) break;  // empty last line
        if (skipEmptyLines && tokenizer.skip_eol()) continue;
        tch = tlineStart;  // in case white space at the beginning may need to be included in field
      }
      else if (tokenizer.skip_eol() && j < ncols) {
        tokenizer.target += preframe_.column(j).is_in_buffer();
        j++;
        if (j==ncols) { used_nrows++; continue; }  // next line
        tch--;
      }
      else {
        tch = fieldStart;
      }
    }
    //*** END TEPID. NOW COLD.


    if (sep==' ') {
      while (tch < tokenizer.eof && *tch==' ') tch++;
      fieldStart = tch;
      if (skipEmptyLines && tokenizer.skip_eol()) continue;
    }

    if (fillme || (tch == tokenizer.eof || (*tch!='\n' && *tch!='\r'))) {  // also includes the case when sep==' '
      while (j < ncols) {
        fieldStart = tch;
        auto ptype_iter = preframe_.column(j).get_ptype_iterator(&tokenizer.quoteRule);

        while (true) {
          tch = fieldStart;
          bool quoted = false;
          if (!ParserLibrary::info(*ptype_iter).isstring()) {
            tokenizer.skip_whitespace();
            const char* afterSpace = tch;
            tch = tokenizer.end_NA_string(tch);
            tokenizer.skip_whitespace();
            if (!tokenizer.at_end_of_field()) tch = afterSpace;
            if (tch < tokenizer.eof && *tch==quote) { quoted=true; tch++; }
          }
          parsers[*ptype_iter](tokenizer);
          if (quoted) {
            if (tch < tokenizer.eof && *tch==quote) tch++;
            else goto typebump;
          }
          tokenizer.skip_whitespace();
          if (tokenizer.at_end_of_field()) {
            if (sep==' ' && tch < tokenizer.eof && *tch==' ') {
              while ((tch + 1 < tokenizer.eof) && tch[1]==' ') tch++;  // multiple space considered one sep so move to last
              if (((tch + 1 < tokenizer.eof) && (tch[1]=='\r' || tch[1]=='\n'))
                  || (tch + 1 == tokenizer.eof)) tch++;
            }
            break;
          }

          // Only perform bumping types / quote rules, when we are sure that the
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
        // is can only happen to one thread at a time. Thus, there is no need
        // for "critical" section here.
        auto& colj = preframe_.column(j);
        if (ptype_iter.has_incremented()) {
          xassert(cc.is_start_exact());
          if (verbose) {
            freader.fo.type_bump_info(j + 1, colj, *ptype_iter, fieldStart,
                                      tch - fieldStart,
                                      static_cast<int64_t>(row0 + used_nrows));
          }
          types[j] = *ptype_iter;
          colj.set_ptype(types[j]);
          if (!freader.reread_scheduled) {
            freader.reread_scheduled = true;
            freader.job->add_work_amount(GenericReader::WORK_REREAD);
          }
        }
        tokenizer.target += colj.is_in_buffer();
        j++;
        if (tch < tokenizer.eof && *tch==sep) { tch++; continue; }
        if (fill && (tch == tokenizer.eof || *tch=='\n' || *tch=='\r') && j <= ncols) {
          // All parsers have already stored NA to target; except for string
          // which writes "" value instead -- hence this case should be
          // corrected here.
          if (colj.is_string() && colj.is_in_buffer() &&
              tokenizer.target[-1].str32.length == 0) {
            tokenizer.target[-1].str32.setna();
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
        tokenizer.skip_whitespace_at_line_start();
        while (tokenizer.skip_eol()) {
          tokenizer.skip_whitespace();
        }
        if (tokenizer.ch == tokenizer.eof) break;
      }

      // not enough columns observed (including empty line). If fill==true,
      // fields should already have been filled above due to continue inside
      // `while (j < ncols)`.
      if (cc.is_start_exact()) {
        throw IOError() << "Too few fields on line "
          << row0 + used_nrows + freader.line
          << ": expected " << ncols << " but found only " << j
          << " (with sep='" << sep << "'). Set fill=True to ignore this error. "
          << " <<" << freader.repr_source(tlineStart, 500) << ">>";
      } else {
        return;
      }
    }

    if (!(tokenizer.skip_eol() || tch == tokenizer.eof)) {
      if (cc.is_start_exact()) {
        throw IOError() << "Too many fields on line "
          << row0 + used_nrows + freader.line
          << ": expected " << ncols << " but more are present. <<"
          << freader.repr_source(tlineStart, 500) << ">>";
      } else {
        return;
      }
    }
    used_nrows++;
  }

  postprocess();

  // Tell the caller where we finished reading the chunk. This is why
  // the parameter `actual_cc` was passed to this function.
  actual_cc.set_end_exact(tch);
  if (verbose) ttime_read += wallclock() - t0;
}


void FreadThreadContext::postprocess() {
  const uint8_t* zanchor = reinterpret_cast<const uint8_t*>(anchor);
  uint8_t echar = quoteRule == 0? static_cast<uint8_t>(quote) :
                  quoteRule == 1? '\\' : 0xFF;
  uint32_t output_offset = 0;
  for (size_t i = 0, j = 0; i < preframe_.ncols(); ++i) {
    auto& col = preframe_.column(i);
    if (!col.is_in_buffer()) continue;
    if (col.is_string() && !col.is_type_bumped()) {
      strinfo[j].start = output_offset;
      field64* coldata = tbuf.data() + j;
      for (size_t n = 0; n < used_nrows; ++n) {
        // Initially, offsets of all entries are given relative to `zanchor`.
        // If a string is NA, its length will be INT_MIN.
        uint32_t entry_offset = coldata->str32.offset;
        int32_t entry_length = coldata->str32.length;
        if (entry_length > 0) {
          size_t zlen = static_cast<size_t>(entry_length);
          if (sbuf.size() < zlen * 3 + output_offset) {
            sbuf.resize(size_t((2 - 1.0*n/used_nrows)*sbuf.size()) + zlen*3);
          }
          uint8_t* dest = sbuf.data() + output_offset;
          const uint8_t* src = zanchor + entry_offset;
          int res = check_escaped_string(src, zlen, echar);
          int32_t newlen = entry_length;
          if (res == 0) {
            // The most common case: the string is correct UTF-8 and does not
            // require un-escaping. Leave the entry as-is
            std::memcpy(dest, src, zlen);
          } else if (res == 1) {
            // Valid UTF-8, but requires un-escaping
            newlen = decode_escaped_csv_string(src, entry_length, dest, echar);
          } else {
            // Invalid UTF-8
            newlen = decode_win1252(src, entry_length, dest);
            xassert(newlen > 0);
            newlen = decode_escaped_csv_string(dest, newlen, dest, echar);
          }
          xassert(newlen > 0);
          output_offset += static_cast<uint32_t>(newlen);
          coldata->str32.length = newlen;
          coldata->str32.offset = output_offset;
        } else if (entry_length == 0) {
          coldata->str32.offset = output_offset;
        } else {
          xassert(coldata->str32.isna());
          coldata->str32.offset = output_offset ^ GETNA<uint32_t>();
        }
        coldata += tbuf_ncols;
        xassert(output_offset <= sbuf.size());
      }
    }
    ++j;
  }
}




void FreadThreadContext::push_buffers() {
  double t0 = verbose? wallclock() : 0;
  ThreadContext::push_buffers();
  if (verbose) ttime_push += wallclock() - t0;
}




}}  // namespace dt::read
