//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/reader_fread.h"      // FreadReader
#include "encodings.h"             // check_escaped_string, decode_escaped_csv_string
#include "py_encodings.h"          // decode_win1252
#include "read/fread/fread_thread_context.h"
#include "read/parallel_reader.h"  // ChunkCoordinates
#include "utils/misc.h"            // wallclock

namespace dt {
namespace read {



FreadThreadContext::FreadThreadContext(
    size_t bcols, size_t brows, FreadReader& f, PT* types_,
    dt::shared_mutex& mut
  ) : ThreadContext(bcols, brows),
      types(types_),
      freader(f),
      columns(f.columns),
      shmutex(mut),
      tokenizer(f.makeTokenizer(tbuf.data(), nullptr)),
      parsers(ParserLibrary::get_parser_fns())
{
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

  size_t ncols = columns.size();
  bool fillme = fill || (columns.size()==1 && !skipEmptyLines);
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
        if (*tch != sep) break;
        tokenizer.target += columns[j].is_in_buffer();
        tch++;
        j++;
      }
      //*** END HOT. START TEPID ***//
      if (tch == tlineStart) {
        tokenizer.skip_whitespace_at_line_start();
        if (*tch == '\0') break;  // empty last line
        if (skipEmptyLines && tokenizer.skip_eol()) continue;
        tch = tlineStart;  // in case white space at the beginning may need to be included in field
      }
      else if (tokenizer.skip_eol() && j < ncols) {
        tokenizer.target += columns[j].is_in_buffer();
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
      while (*tch==' ') tch++;
      fieldStart = tch;
      if (skipEmptyLines && tokenizer.skip_eol()) continue;
    }

    if (fillme || (*tch!='\n' && *tch!='\r')) {  // also includes the case when sep==' '
      while (j < ncols) {
        fieldStart = tch;
        auto ptype_iter = columns[j].get_ptype_iterator(&tokenizer.quoteRule);

        while (true) {
          tch = fieldStart;
          bool quoted = false;
          if (!ParserLibrary::info(*ptype_iter).isstring()) {
            tokenizer.skip_whitespace();
            const char* afterSpace = tch;
            tch = tokenizer.end_NA_string(tch);
            tokenizer.skip_whitespace();
            if (!tokenizer.at_end_of_field()) tch = afterSpace;
            if (*tch==quote) { quoted=true; tch++; }
          }
          parsers[*ptype_iter](tokenizer);
          if (quoted) {
            if (*tch==quote) tch++;
            else goto typebump;
          }
          tokenizer.skip_whitespace();
          if (tokenizer.at_end_of_field()) {
            if (sep==' ' && *tch==' ') {
              while (tch[1]==' ') tch++;  // multiple space considered one sep so move to last
              if (tch[1]=='\r' || tch[1]=='\n' || tch[1]=='\0') tch++;
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
        if (ptype_iter.has_incremented()) {
          xassert(cc.is_start_exact());
          if (verbose) {
            freader.fo.type_bump_info(j + 1, columns[j], *ptype_iter, fieldStart,
                                      tch - fieldStart,
                                      static_cast<int64_t>(row0 + used_nrows));
          }
          types[j] = *ptype_iter;
          columns[j].set_ptype(ptype_iter);
          if (!freader.reread_scheduled) {
            freader.reread_scheduled = true;
            freader.job->add_work_amount(GenericReader::WORK_REREAD);
          }
        }
        tokenizer.target += columns[j].is_in_buffer();
        j++;
        if (*tch==sep) { tch++; continue; }
        if (fill && (*tch=='\n' || *tch=='\r' || *tch=='\0') && j <= ncols) {
          // All parsers have already stored NA to target; except for string
          // which writes "" value instead -- hence this case should be
          // corrected here.
          if (columns[j-1].is_string() && columns[j-1].is_in_buffer() &&
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
        if (tokenizer.at_eof()) break;
      }

      // not enough columns observed (including empty line). If fill==true,
      // fields should already have been filled above due to continue inside
      // `while (j < ncols)`.
      if (cc.is_start_exact()) {
        throw RuntimeError() << "Too few fields on line "
          << row0 + used_nrows + freader.line
          << ": expected " << ncols << " but found only " << j
          << " (with sep='" << sep << "'). Set fill=True to ignore this error. "
          << " <<" << freader.repr_source(tlineStart, 500) << ">>";
      } else {
        return;
      }
    }
    if (!(tokenizer.skip_eol() || *tch=='\0')) {
      if (cc.is_start_exact()) {
        throw RuntimeError() << "Too many fields on line "
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
  for (size_t i = 0, j = 0; i < columns.size(); ++i) {
    Column& col = columns[i];
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


void FreadThreadContext::orderBuffer() {
  if (!used_nrows) return;
  for (size_t i = 0, j = 0; i < columns.size(); ++i) {
    Column& col = columns[i];
    if (!col.is_in_buffer()) continue;
    if (col.is_string() && !col.is_type_bumped()) {
      // Compute the size of the string content in the buffer `sz` from the
      // offset of the last element. This quantity cannot be calculated in the
      // postprocess() step, since `used_nrows` may some times change affecting
      // this size after the post-processing.
      uint32_t offset0 = static_cast<uint32_t>(strinfo[j].start);
      uint32_t offsetL = tbuf[j + tbuf_ncols * (used_nrows - 1)].str32.offset;
      size_t sz = (offsetL - offset0) & ~GETNA<uint32_t>();
      strinfo[j].size = sz;

      WritableBuffer* wb = col.strdata_w();
      size_t write_at = wb->prep_write(sz, sbuf.data() + offset0);
      strinfo[j].write_at = write_at;
    }
    ++j;
  }
}


void FreadThreadContext::push_buffers() {
  // If the buffer is empty, then there's nothing to do...
  if (!used_nrows) return;
  dt::shared_lock<dt::shared_mutex> lock(shmutex);

  double t0 = verbose? wallclock() : 0;
  size_t ncols = columns.size();
  for (size_t i = 0, j = 0; i < ncols; i++) {
    Column& col = columns[i];
    if (!col.is_in_buffer()) continue;
    void* data = col.data_w();
    int8_t elemsize = static_cast<int8_t>(col.elemsize());

    if (col.is_type_bumped()) {
      // do nothing: the column was not properly allocated for its type, so
      // any attempt to write the data may fail with data corruption
    } else if (col.is_string()) {
      WritableBuffer* wb = col.strdata_w();
      SInfo& si = strinfo[j];
      field64* lo = tbuf.data() + j;

      wb->write_at(si.write_at, si.size, sbuf.data() + si.start);

      if (elemsize == 4) {
        uint32_t* dest = static_cast<uint32_t*>(data) + row0 + 1;
        uint32_t delta = static_cast<uint32_t>(si.write_at - si.start);
        for (size_t n = 0; n < used_nrows; ++n) {
          uint32_t soff = lo->str32.offset;
          *dest++ = soff + delta;
          lo += tbuf_ncols;
        }
      } else {
        uint64_t* dest = static_cast<uint64_t*>(data) + row0 + 1;
        uint64_t delta = static_cast<uint64_t>(si.write_at - si.start);
        for (size_t n = 0; n < used_nrows; ++n) {
          uint64_t soff = lo->str32.offset;
          *dest++ = soff + delta;
          lo += tbuf_ncols;
        }
      }

    } else {
      const field64* src = tbuf.data() + j;
      if (elemsize == 8) {
        uint64_t* dest = static_cast<uint64_t*>(data) + row0;
        for (size_t r = 0; r < used_nrows; r++) {
          *dest = src->uint64;
          src += tbuf_ncols;
          dest++;
        }
      } else
      if (elemsize == 4) {
        uint32_t* dest = static_cast<uint32_t*>(data) + row0;
        for (size_t r = 0; r < used_nrows; r++) {
          *dest = src->uint32;
          src += tbuf_ncols;
          dest++;
        }
      } else
      if (elemsize == 1) {
        uint8_t* dest = static_cast<uint8_t*>(data) + row0;
        for (size_t r = 0; r < used_nrows; r++) {
          *dest = src->uint8;
          src += tbuf_ncols;
          dest++;
        }
      }
    }
    j++;
  }
  used_nrows = 0;
  if (verbose) ttime_push += wallclock() - t0;
}



}  // namespace read
}  // namespace dt
