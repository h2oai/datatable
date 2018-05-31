//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/reader_fread.h"
#include <string.h>    // memcpy
#include <sys/mman.h>  // mmap
#include <cmath>       // std::exp
#include <exception>
#include <map>         // std::map
#include "csv/reader.h"
#include "csv/reader_parsers.h"
#include "csv/fread.h"
#include "datatable.h"
#include "column.h"
#include "py_datatable.h"
#include "py_encodings.h"
#include "py_utils.h"
#include "utils/assert.h"
#include "utils/file.h"
#include "utils/pyobj.h"
#include "utils.h"



//------------------------------------------------------------------------------
// Initialization
//------------------------------------------------------------------------------

FreadReader::FreadReader(const GenericReader& g)
  : GenericReader(g), parsers(parserlib.get_parser_fns()), fo(g)
{
  size_t input_size = datasize();
  targetdir = nullptr;
  // TODO: Do not require the extra byte, and do not write into the input stream...
  xassert(extra_byte_accessible());
  xassert(input_size > 0);
  // Usually the extra byte is already zero, however if we skipped whitespace
  // at the end, it may no longer be so
  *const_cast<char*>(eof) = '\0';

  first_jump_size = 0;
  n_sample_lines = 0;
  whiteChar = '\0';
  quoteRule = -1;
  LFpresent = false;
  fo.input_size = input_size;
}

FreadReader::~FreadReader() {
  // free(strbufs);
}


FreadTokenizer FreadReader::makeTokenizer(
    field64* target, const char* anchor) const
{
  return {
    .ch = nullptr,
    .target = target,
    .anchor = anchor,
    .eof = eof,
    .NAstrings = na_strings,
    .whiteChar = whiteChar,
    .dec = dec,
    .sep = sep,
    .quote = quote,
    .quoteRule = quoteRule,
    .strip_whitespace = strip_whitespace,
    .blank_is_na = blank_is_na,
    .LFpresent = LFpresent,
  };
}



//------------------------------------------------------------------------------
// Detect the separator / quoting rule
//
// This entire section is WIP
//------------------------------------------------------------------------------
class HypothesisNoQC;
class HypothesisQC;
class HypothesisPool;

class Hypothesis {
  protected:
    FreadTokenizer& ctx;
    size_t nlines;
    bool invalid;
    int64_t : 56;

  public:
    Hypothesis(FreadTokenizer& c) : ctx(c), nlines(0), invalid(false) {}
    virtual ~Hypothesis() {}
    virtual void parse_next_line(HypothesisPool&) = 0;
    virtual double score() = 0;
    friend HypothesisPool;
};

class HypothesisPool : public std::vector<Hypothesis*> {
  public:
    static const size_t MaxLines = 100;

    void parse_next_line() {
      // Dynamic `size()`: in case any new hypotheses are inserted, they are
      // checked too.
      for (size_t i = 0; i < size(); ++i) {
        Hypothesis* h = (*this)[i];
        if (h->invalid) continue;
        h->parse_next_line(*this);
      }
    }
};

class HypothesisQC : public Hypothesis {
  private:
    HypothesisNoQC* parent;
    char qc;
    int64_t : 56;
  public:
    HypothesisQC(FreadTokenizer& c, char q, HypothesisNoQC* p)
      : Hypothesis(c), parent(p), qc(q) {}
    void parse_next_line(HypothesisPool&) override {
      (void) parent;
      (void) qc;
    }
    double score() override { return 0.5; }
};

class HypothesisNoQC : public Hypothesis {
  private:
    static const size_t MaxSeps = 128;
    std::vector<size_t> chcounts;
    std::map<size_t, size_t> metacounts;  // used for scoring
    bool doubleQuoteSeen;
    bool singleQuoteSeen;
    int64_t : 48;
  public:
    HypothesisNoQC(FreadTokenizer& ctx)
      : Hypothesis(ctx), chcounts(MaxSeps * HypothesisPool::MaxLines),
        doubleQuoteSeen(false), singleQuoteSeen(false) {}

    void parse_next_line(HypothesisPool& hp) override {
      const char*& ch = ctx.ch;
      const char* eof = ctx.eof;
      size_t* chfreq = &(chcounts[nlines * MaxSeps]);
      while (ch < eof && *ch == ' ') ch++;
      size_t nspaces = 0; // the number of contiguous spaces seen before now
      while (ch < eof) {
        signed char c = *ch;
        if (c >= 0) {  // non-ASCII range will have `c < 0`
          chfreq[c]++;
          chfreq[+'s'] += (c == ' ' && nspaces == 0) - (c == 's');
          if (c == '\n' || c == '\r') {
            if (ctx.skip_eol()) {
              chfreq[+' '] -= nspaces;
              chfreq[+'s'] -= (nspaces > 0);
              break;
            }
          }
        }
        nspaces = (c == ' ')? nspaces + 1 : 0;
        ch++;
      }
      if (!doubleQuoteSeen && chfreq[+'"']) {
        hp.push_back(new HypothesisQC(ctx, '"', this));
        doubleQuoteSeen = true;
      }
      if (!singleQuoteSeen && chfreq[+'\'']) {
        hp.push_back(new HypothesisQC(ctx, '\'', this));
        singleQuoteSeen = true;
      }
      nlines++;
    }
    double score() override {
      if (invalid) return 0;
      for (size_t i = 0; i < MaxSeps; ++i) {
        if (!allowedseps[i]) continue;
        score_sep(i);
        if (i == ' ' || !(allowedseps[i] || i == 's')) continue;
      }
      return 0;
    }
    double score_sep(size_t sep) {
      metacounts.clear();
      size_t off = sep;
      double sep_weight = static_cast<double>(allowedseps[sep]);
      if (sep == ' ') {
        off = 's';
        size_t cnt_space = 0, cnt_multispace = 0;
        for (size_t l = 0; l < nlines; l++) {
          cnt_space += chcounts[' ' + l * MaxSeps];
          cnt_multispace += chcounts['s' + l * MaxSeps];
        }
        double avg_len = static_cast<double>(cnt_space) / cnt_multispace;
        sep_weight *= 2.0 / (1 + std::exp(2 - avg_len));
      }
      for (size_t l = 0; l < nlines; l++) {
        metacounts[chcounts[off + l * MaxSeps]]++;
      }
      size_t max_count_of_counts = 0, best_count = 0;
      for (auto m : metacounts) {
        if (m.second > max_count_of_counts) {
          max_count_of_counts = m.second;
          best_count = m.first;
        }
      }
      return sep_weight;
    }
};

/**
 * QR = 0: no embedded quote chars allowed
 * QR = 1: embedded quote characters are doubled
 * QR = 2: embedded quote characters are escaped with '\'
 *
 * Consider 3 hypothesis:
 * H0: QC = ø
 * H1: QC = «"», starting with QR1 = 0
 * H2: QC = «'», starting with QR2 = 0
 */
void FreadReader::detect_sep(FreadTokenizer&) {
}



//------------------------------------------------------------------------------
// Column type detection
//------------------------------------------------------------------------------

/**
 * Helper class to facilitate chunking during type-detection.
 */
class ColumnTypeDetectionChunkster {
  public:
    const FreadReader& f;
    FreadChunkedReader fcr;
    FreadTokenizer fctx;
    size_t nchunks;
    size_t chunk_distance;
    const char* last_row_end;

    ColumnTypeDetectionChunkster(FreadReader& fr, FreadTokenizer& ft)
    : f(fr), fcr(fr, nullptr), fctx(ft)
    {
      nchunks = 0;
      chunk_distance = 0;
      last_row_end = f.sof;
      determine_chunking_strategy();
    }

    void determine_chunking_strategy() {
      size_t chunk0_size = f.first_jump_size;
      size_t input_size = static_cast<size_t>(f.eof - f.sof);
      if (chunk0_size == 0 || chunk0_size == input_size) {
        nchunks = 1;
        f.trace("Number of sampling jump points = 1 because input is less "
                "than 100 lines");
      } else {
        xassert(chunk0_size < input_size);
        nchunks = chunk0_size * 200 < input_size ? 101 :
                  chunk0_size * 20  < input_size ? 11 : 1;
        if (nchunks > 1) chunk_distance = input_size / (nchunks - 1);
        f.trace("Number of sampling jump points = %zu because the first "
                "chunk was %.1f times smaller than the entire file",
                nchunks, 1.0 * input_size / chunk0_size);
      }
    }

    ChunkCoordinates compute_chunk_boundaries(size_t j) {
      ChunkCoordinates cc(f.eof, f.eof);
      if (j == 0) {
        if (f.header == 0) {
          cc.start = f.sof;
        } else {
          // If `header` is either True or <auto>, we skip the first row during
          // type detection.
          fctx.ch = f.sof;
          int n = fctx.countfields();
          if (n >= 0) cc.start = fctx.ch;
        }
      } else {
        const char* tch =
          (j == nchunks - 1) ? f.eof - f.first_jump_size/2 :
                               f.sof + j * chunk_distance;
        if (tch < last_row_end) tch = last_row_end;

        // Skip any potential newlines, in case we jumped in the middle of one.
        // In particular, it could be problematic if the file had '\n\r' newline
        // and we jumped onto the second '\r' (which wouldn't be considered a
        // newline by `skip_eol()`s rules, which would then become a part of the
        // following field).
        while (*tch == '\n' || *tch == '\r') tch++;

        if (tch < f.eof) {
          bool ok = fcr.next_good_line_start(cc, fctx);
          if (ok) cc.start = fctx.ch;
        }
      }
      return cc;
    }
};


/**
 * Parse a single line of input, discarding the parsed values but detecting
 * the proper column types. This method will bump `columns[j].type`s if
 * necessary in order to parse the fields. It will advance the parse location
 * to the beginning of the next line, and return the number of fields detected
 * on the line (which could be more or less than the number of columns).
 *
 * If the line is empty then 0 is returned (the caller should try to
 * disambiguate this from a situation of a single column with NA field).
 *
 * If the line cannot be parsed (because it contains a string that is not
 * parseable under the current quoting rule), then return -1.
 */
int64_t FreadReader::parse_single_line(FreadTokenizer& fctx)
{
  const char*& tch = fctx.ch;

  // detect blank lines
  fctx.skip_whitespace_at_line_start();
  if (tch == eof || fctx.skip_eol()) return 0;

  size_t ncols = columns.size();
  size_t j = 0;
  while (true) {
    fctx.skip_whitespace();

    const char* fieldStart = tch;
    PT coltype = j < ncols ? columns[j].type : PT::Str32;
    while (true) {
      // Try to parse using the regular field parser
      tch = fieldStart;
      parsers[coltype](fctx);
      fctx.skip_whitespace();
      if (fctx.end_of_field()) break;

      // Try to parse as NA
      // TODO: this API is awkward; better have smth like `fctx.parse_na();`
      tch = fctx.end_NA_string(fieldStart);
      fctx.skip_whitespace();
      if (fctx.end_of_field()) break;

      if (ParserLibrary::info(coltype).isstring()) {
        // Do not bump the quote rule, since we cannot be sure that the jump
        // was reliable. Instead, we'll defer quote rule bumping to regular
        // file reading.
        return -1;
      }

      // Try to parse as quoted field
      if (*fieldStart == quote) {
        tch = fieldStart + 1;
        parsers[coltype](fctx);
        if (*tch == quote) {
          tch++;
          fctx.skip_whitespace();
          if (fctx.end_of_field()) break;
        }
      }

      // Finally, bump the column's type and try again
      // TODO: replace with proper PT iteration
      coltype = static_cast<PT>(coltype + 1);
      if (j < ncols) columns[j].type = coltype;
    }
    j++;

    if (*tch == sep) {
      if (sep == ' ') {
        // Multiple spaces are considered a single sep. In addition, spaces at
        // the end of the line should be discarded and not treated as a sep.
        while (*tch == ' ') tch++;
        if (fctx.skip_eol()) break;
      } else {
        tch++;
      }
    } else if (fctx.skip_eol() || tch == eof) {
      break;
    } else {
      xassert(0 && "Invalid state when parsing a line");
    }
  }
  return static_cast<int64_t>(j);
}


void FreadReader::detect_column_types()
{
  trace("[3] Detect column types and header");
  size_t ncols = columns.size();
  int64_t sncols = static_cast<int64_t>(ncols);

  field64 tmp;
  FreadTokenizer fctx = makeTokenizer(&tmp, nullptr);
  const char*& tch = fctx.ch;

  ColumnTypeDetectionChunkster chunkster(*this, fctx);
  size_t nChunks = chunkster.nchunks;

  double sumLen = 0.0;
  double sumLenSq = 0.0;
  int minLen = INT32_MAX;   // int_max so the first if(thisLen<minLen) is always true; similarly for max
  int maxLen = -1;

  // Start with all columns having the smallest possible type
  columns.setType(PT::Mu);

  // This variable will store column types at the beginning of each jump
  // so that we can revert to them if the jump proves to be invalid.
  std::unique_ptr<PT[]> saved_types(new PT[ncols]);

  for (size_t j = 0; j < nChunks; ++j) {
    ChunkCoordinates cc = chunkster.compute_chunk_boundaries(j);
    tch = cc.start;
    if (tch >= eof) continue;

    columns.saveTypes(saved_types);

    for (int i = 0; i < JUMPLINES; ++i) {
      if (tch >= eof) break;
      const char* lineStart = tch;
      int64_t incols = parse_single_line(fctx);
      if (incols == 0 && (skip_blank_lines || ncols == 1)) {
        continue;
      }
      // bool eol_found = fctx.skip_eol();
      if (incols == -1 || (incols != sncols && !fill)) {
        trace("A line with too %s fields (%zd out of %zd) was found on line %d "
              "of sample jump %zu",
              incols < sncols ? "few" : "many", incols, ncols, i, j);
        // Restore column types: it is possible that the chunk start was guessed
        // incorrectly, in which case we don't want the types to be bumped
        // invalidly. This applies to all chunks except the first (for which we
        // know that the start is correct).
        if (j == 0) {
          chunkster.last_row_end = eof;
        } else {
          columns.setTypes(saved_types);
        }
        break;
      }
      n_sample_lines++;
      chunkster.last_row_end = tch;
      int thisLineLen = (int)(tch - lineStart);
      xassert(thisLineLen >= 0);
      sumLen += thisLineLen;
      sumLenSq += thisLineLen*thisLineLen;
      if (thisLineLen<minLen) minLen = thisLineLen;
      if (thisLineLen>maxLen) maxLen = thisLineLen;
    }
    if (verbose && (j == 0 || j == nChunks - 1 ||
                    !columns.sameTypes(saved_types))) {
      trace("Type codes (jump %03d): %s", j, columns.printTypes());
    }
  }

  detect_header();

  size_t estnrow = 1;
  allocnrow = 1;
  meanLineLen = 0;

  if (n_sample_lines <= 1) {
    if (header == 1) {
      // A single-row input, and that row is the header. Reset all types to
      // boolean (lowest type possible, a better guess than "string").
      columns.setType(PT::Mu);
      allocnrow = 0;
    }
    meanLineLen = sumLen;
  } else {
    size_t bytesRead = static_cast<size_t>(eof - sof);
    meanLineLen = sumLen/n_sample_lines;
    estnrow = static_cast<size_t>(std::ceil(bytesRead/meanLineLen));  // only used for progress meter and verbose line below
    double sd = std::sqrt( (sumLenSq - (sumLen*sumLen)/n_sample_lines)/(n_sample_lines-1) );
    allocnrow = std::max(static_cast<size_t>(bytesRead / fmax(meanLineLen - 2*sd, minLen)),
                         static_cast<size_t>(1.1*estnrow));
    allocnrow = std::min(allocnrow, 2*estnrow);
    // sd can be very close to 0.0 sometimes, so apply a +10% minimum
    // blank lines have length 1 so for fill=true apply a +100% maximum. It'll be grown if needed.
    if (verbose) {
      trace("=====");
      trace("Sampled %zd rows (handled \\n inside quoted fields) at %d jump point(s)", n_sample_lines, nChunks);
      trace("Bytes from first data row to the end of last row: %zd", bytesRead);
      trace("Line length: mean=%.2f sd=%.2f min=%d max=%d", meanLineLen, sd, minLen, maxLen);
      trace("Estimated number of rows: %zd / %.2f = %zd", bytesRead, meanLineLen, estnrow);
      trace("Initial alloc = %zd rows (%zd + %d%%) using bytes/max(mean-2*sd,min) clamped between [1.1*estn, 2.0*estn]",
            allocnrow, estnrow, (int)(100.0*allocnrow/estnrow-100.0));
    }
    if (nChunks==1) {
      if (header == 1) n_sample_lines--;
      estnrow = allocnrow = n_sample_lines;
      trace("All rows were sampled since file is small so we know nrows=%zd exactly", estnrow);
    } else {
      xassert(n_sample_lines <= allocnrow);
    }
    if (max_nrows < allocnrow) {
      trace("Alloc limited to nrows=%zd according to the provided max_nrows argument.", max_nrows);
      estnrow = allocnrow = max_nrows;
    }
    trace("=====");
  }
  fo.n_lines_sampled = n_sample_lines;
}


/**
 * Detect whether the first line in input is the header or not.
 */
void FreadReader::detect_header() {
  if (!ISNA<int8_t>(header)) return;
  size_t ncols = columns.size();
  int64_t sncols = static_cast<int64_t>(ncols);

  field64 tmp;
  FreadTokenizer fctx = makeTokenizer(&tmp, nullptr);
  const char*& tch = fctx.ch;

  // Detect types in the header column
  auto saved_types = columns.getTypes();
  tch = sof;
  columns.setType(PT::Mu);
  int64_t ncols_header = parse_single_line(fctx);
  auto header_types = columns.getTypes();
  columns.setTypes(saved_types);

  if (ncols_header != sncols && n_sample_lines > 0 && !fill) {
    header = true;
    trace("`header` determined to be True because the first line contains "
          "different number of columns (%zd) than the rest of the file (%zu)",
          ncols_header, ncols);
    if (ncols_header > sncols) {
      fill = true;
      trace("Setting `fill` to True because the header contains more columns "
            "than the data.");
      for (int64_t j = sncols; j < ncols_header; ++j) {
        columns.push_back(GReaderColumn());
      }
    }
    return;
  }

  if (n_sample_lines > 0) {
    for (size_t j = 0; j < ncols; ++j) {
      if (ParserLibrary::info(header_types[j]).isstring() &&
          !ParserLibrary::info(saved_types[j]).isstring() &&
          saved_types[j] != PT::Mu) {
        header = true;
        trace("`header` determined to be True due to column %d containing a "
              "string on row 1 and type %s in the rest of the sample.",
              j+1, ParserLibrary::info(saved_types[j]).cname());
        return;
      }
    }
  }

  bool all_strings = true;
  for (size_t j = 0; j < ncols; ++j) {
    if (!ParserLibrary::info(header_types[j]).isstring())
      all_strings = false;
  }
  if (all_strings) {
    header = true;
    trace("`header` determined to be True because all inputs columns are "
          "strings and better guess is not possible");
  } else {
    header = false;
    trace("`header` determined to be False because some of the fields on "
          "the first row are not of the string type");
    // If header is false, then the first row also belongs to the sample.
    // Accurate count of sample lines is needed so that we can allocate
    // the correct amount of rows for the output Frame.
    n_sample_lines++;
  }
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

/**
 * This helper method tests whether '\\n' characters are present in the file,
 * and sets the `LFpresent` flag accordingly.
 *
 * If '\\n' exists in the file, then `LFpresent` is set to true, and standalone
 * '\\r' will be treated as a regular character. However if there are no '\\n's
 * in the file (at least within the first 100 lines), then we will treat '\\r'
 * as a newline character.
 */
void FreadReader::detect_lf() {
  int cnt = 0;
  const char* ch = sof;
  while (ch < eof && *ch != '\n' && cnt < 100) {
    cnt += (*ch == '\r');
    ch++;
  }
  LFpresent = (ch < eof && *ch == '\n');
  cr_is_newline = !LFpresent;
  if (LFpresent) {
    trace("LF character (\\n) found in input, "
          "\\r-only newlines will not be recognized");
  } else {
    trace("LF character (\\n) not found in input, "
          "CR character (\\r) will be treated as a newline");
  }

}


/**
 * Detect whether the file contains an initial "preamble" section (comments
 * at the top of the file), and if so skip them.
 */
void FreadReader::skip_preamble() {
  if (skip_to_line || skip_to_string) {
    // If the user has explicitly requested skip then do not try to detect
    // any other comment section.
    return;
  }

  field64 tmp;
  auto fctx = makeTokenizer(&tmp, /* anchor = */ nullptr);
  const char*& ch = fctx.ch;

  char comment_char = '\xFF';  // meaning "auto"
  size_t comment_lines = 0;
  size_t total_lines = 0;

  ch = sof;
  while (ch < eof) {
    const char* start_of_line = ch;
    total_lines++;
    fctx.skip_whitespace_at_line_start();
    if (fctx.skip_eol()) continue;
    if (comment_char == '\xFF') {
      if (*ch == '#' || *ch == '%') comment_char = *ch;
    }
    if (*ch == comment_char) {
      comment_lines++;
      while (ch < eof) {
        if ((*ch == '\n' || *ch == '\r') && fctx.skip_eol()) break;
        ch++;
      }
    } else {
      ch = start_of_line;
      total_lines--;
      break;
    }
  }
  if (comment_lines) {
    trace("Comment section (%zu line%s starting with '%c') found at the "
          "top of the file and skipped",
          comment_lines, (comment_lines == 1? "" : "s"), comment_char);
    sof = ch;
    line += total_lines;
  }
}



/**
 * Parse a single line of input starting from location `ctx.ch` as strings,
 * and interpret them as column names. At the end of this function the parsing
 * location `ctx.ch` will be moved to the beginning of the next line.
 *
 * The column names will be stored in `columns[i].name` fields. If the number
 * of column names on the input line is greater than the number of `columns`,
 * then the `columns` array will be extended to accomodate extra columns. If
 * the number of column names is less than the number of allocated columns,
 * then the missing columns will retain their default empty names.
 *
 * This function assumes that the `quoteRule` and `quote` were already detected
 * correctly, so that `parse_string()` can parse each field without error. If
 * not, a `RuntimeError` will be thrown.
 */
// TODO name-cleaning should be a method of GReaderColumn
void FreadReader::parse_column_names(FreadTokenizer& ctx) {
  const char*& ch = ctx.ch;

  // Skip whitespace at the beginning of a line.
  if (strip_whitespace && (*ch == ' ' || (*ch == '\t' && sep != '\t'))) {
    while (*ch == ' ' || *ch == '\t') ch++;
  }

  uint8_t echar = quoteRule == 0? static_cast<uint8_t>(quote) :
                  quoteRule == 1? '\\' : 0xFF;

  size_t ncols = columns.size();
  size_t ncols_found;
  for (size_t i = 0; ; ++i) {
    // Parse string field, but do not advance `ctx.target`: on the next
    // iteration we will write into the same place.
    parse_string(ctx);
    const char* start = ctx.anchor + ctx.target->str32.offset;
    int32_t ilen = ctx.target->str32.length;
    size_t zlen = static_cast<size_t>(ilen);

    if (i >= ncols) {
      columns.push_back(GReaderColumn());
    }
    if (ilen > 0) {
      const uint8_t* usrc = reinterpret_cast<const uint8_t*>(start);
      int res = check_escaped_string(usrc, zlen, echar);
      if (res == 0) {
        columns[i].name = std::string(start, zlen);
      } else {
        char* newsrc = new char[zlen * 4];
        uint8_t* unewsrc = reinterpret_cast<uint8_t*>(newsrc);
        int newlen;
        if (res == 1) {
          newlen = decode_escaped_csv_string(usrc, ilen, unewsrc, echar);
        } else {
          newlen = decode_win1252(usrc, ilen, unewsrc);
          newlen = decode_escaped_csv_string(unewsrc, newlen, unewsrc, echar);
        }
        xassert(newlen > 0);
        columns[i].name = std::string(newsrc, static_cast<size_t>(newlen));
        delete[] newsrc;
      }
    }
    // Skip the separator, handling special case of sep=' ' (multiple spaces are
    // treated as a single separator, and spaces at the beginning/end of line
    // are ignored).
    if (ch < eof && sep == ' ' && *ch == ' ') {
      while (ch < eof && *ch == ' ') ch++;
      if (ch == eof || ctx.skip_eol()) {
        ncols_found = i + 1;
        break;
      }
    } else if (ch < eof && *ch == sep && sep != '\n') {
      ch++;
    } else if (ch == eof || ctx.skip_eol()) {
      ncols_found = i + 1;
      break;
    } else {
      throw RuntimeError() << "Internal error: cannot parse column names";
    }
  }

  if (sep == ' ' && ncols_found == ncols - 1) {
    for (size_t j = ncols - 1; j > 0; j--){
      columns[j].name.swap(columns[j-1].name);
    }
    columns[0].name = "index";
  }
}



//------------------------------------------------------------------------------
// FreadLocalParseContext
//------------------------------------------------------------------------------

FreadLocalParseContext::FreadLocalParseContext(
    size_t bcols, size_t brows, FreadReader& f, PT* types_,
    dt::shared_mutex& mut
  ) : LocalParseContext(bcols, brows),
      types(types_),
      freader(f),
      columns(f.columns),
      shmutex(mut),
      tokenizer(f.makeTokenizer(tbuf, nullptr)),
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
  size_t ncols = columns.size();
  size_t bufsize = std::min(size_t(4096), f.datasize() / (ncols + 1));
  for (size_t i = 0, j = 0; i < ncols; ++i) {
    GReaderColumn& col = columns[i];
    if (!col.presentInBuffer) continue;
    if (col.isstring() && !col.typeBumped) {
      strbufs.push_back(StrBuf(bufsize, j, i));
    }
    ++j;
  }
}

FreadLocalParseContext::~FreadLocalParseContext() {
  #pragma omp atomic update
  freader.fo.time_push_data += ttime_push;
  #pragma omp atomic update
  freader.fo.time_read_data += ttime_read;
  ttime_push = 0;
  ttime_read = 0;
}



void FreadLocalParseContext::read_chunk(
  const ChunkCoordinates& cc, ChunkCoordinates& actual_cc)
{
  double t0 = verbose? wallclock() : 0;
  // If any error in the loop below occurs, we'll do `return;` and the output
  // variable `actual_cc` will contain `.end = nullptr;`.
  actual_cc.start = cc.start;
  actual_cc.end = nullptr;
  size_t ncols = columns.size();
  bool fillme = fill || (columns.size()==1 && !skipEmptyLines);
  bool fastParsingAllowed = (sep != ' ') && !numbersMayBeNAs;
  const char*& tch = tokenizer.ch;
  tch = cc.start;
  used_nrows = 0;
  tokenizer.target = tbuf;
  tokenizer.anchor = anchor = cc.start;

  while (tch < cc.end) {
    if (used_nrows == tbuf_nrows) {
      allocate_tbuf(tbuf_ncols, tbuf_nrows * 3 / 2);
      tokenizer.target = tbuf + used_nrows * tbuf_ncols;
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
        tokenizer.target += columns[j].presentInBuffer;
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
        tokenizer.target += columns[j].presentInBuffer;
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
        PT oldType = types[j];
        PT newType = oldType;

        while (true) {
          tch = fieldStart;
          bool quoted = false;
          if (!ParserLibrary::info(newType).isstring()) {
            tokenizer.skip_whitespace();
            const char* afterSpace = tch;
            tch = tokenizer.end_NA_string(tch);
            tokenizer.skip_whitespace();
            if (!tokenizer.end_of_field()) tch = afterSpace;
            if (*tch==quote) { quoted=true; tch++; }
          }
          parsers[newType](tokenizer);
          if (quoted) {
            if (*tch==quote) tch++;
            else goto typebump;
          }
          tokenizer.skip_whitespace();
          if (tokenizer.end_of_field()) {
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
          if (cc.true_start) {
            // TODO: replace with proper type iteration
            if (newType + 1 < ParserLibrary::num_parsers) {
              newType = static_cast<PT>(newType + 1);
            } else {
              tokenizer.quoteRule++;
            }
            tch = fieldStart;
          } else {
            return;
          }
        }

        // Type-bump. This may only happen if cc.true_start is true, which flag
        // is only set to true on one thread at a time. Thus, there is no need
        // for "critical" section here.
        if (newType != oldType) {
          xassert(cc.true_start);
          if (verbose) {
            freader.fo.type_bump_info(j + 1, columns[j], newType, fieldStart,
                                      tch - fieldStart,
                                      static_cast<int64_t>(row0 + used_nrows));
          }
          types[j] = newType;
          columns[j].type = newType;
          columns[j].typeBumped = true;
        }
        tokenizer.target += columns[j].presentInBuffer;
        j++;
        if (*tch==sep) { tch++; continue; }
        if (fill && (*tch=='\n' || *tch=='\r' || *tch=='\0') && j <= ncols) {
          // All parsers have already stored NA to target; except for string
          // which writes "" value instead -- hence this case should be
          // corrected here.
          if (columns[j-1].isstring() && columns[j-1].presentInBuffer &&
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
      if (cc.true_start) {
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
      if (cc.true_start) {
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
  actual_cc.end = tch;
  if (verbose) ttime_read += wallclock() - t0;
}


void FreadLocalParseContext::postprocess() {
  const uint8_t* zanchor = reinterpret_cast<const uint8_t*>(anchor);
  uint8_t echar = quoteRule == 0? static_cast<uint8_t>(quote) :
                  quoteRule == 1? '\\' : 0xFF;
  size_t nstrcols = strbufs.size();
  for (size_t k = 0; k < nstrcols; ++k) {
    MemoryRange& strdest = strbufs[k].mbuf;
    field64* lo = tbuf + strbufs[k].idx8;
    int32_t off = 1;
    size_t bufsize = strdest.size();
    for (size_t n = 0; n < used_nrows; n++) {
      int32_t len = lo->str32.length;
      if (len > 0) {
        size_t zlen = static_cast<size_t>(len);
        size_t zoff = static_cast<size_t>(off);
        if (bufsize < zlen * 3 + zoff) {
          bufsize = bufsize * 2 + zlen * 3;
          strdest.resize(bufsize);
        }
        const uint8_t* src = zanchor + lo->str32.offset;
        uint8_t* dest = static_cast<uint8_t*>(
                          strdest.wptr(static_cast<size_t>(off) - 1));
        int res = check_escaped_string(src, zlen, echar);
        if (res == 0) {
          memcpy(dest, src, zlen);
          off += zlen;
          lo->str32.offset = off;
        } else if (res == 1) {
          int newlen = decode_escaped_csv_string(src, len, dest, echar);
          off += static_cast<size_t>(newlen);
          lo->str32.offset = off;
        } else {
          int newlen = decode_win1252(src, len, dest);
          xassert(newlen > 0);
          newlen = decode_escaped_csv_string(dest, newlen, dest, echar);
          off += static_cast<size_t>(newlen);
          lo->str32.offset = off;
        }
      } else if (len == 0) {
        lo->str32.offset = off;
      } else {
        xassert(lo->str32.isna());
        lo->str32.offset = -off;
      }
      lo += tbuf_ncols;
    }
    strbufs[k].ptr = static_cast<size_t>(off - 1);
  }
}


void FreadLocalParseContext::orderBuffer() {
  if (!used_nrows) return;
  size_t nstrcols = strbufs.size();
  for (size_t k = 0; k < nstrcols; ++k) {
    size_t i = strbufs[k].idxdt;
    size_t j8 = strbufs[k].idx8;
    // Compute `sz` (the size of the string content in the buffer) from the
    // offset of the last element. Typically this would be the same as
    // `strbufs[k].ptr`, however in rare cases when `used_nrows` have changed
    // from the time the buffer was post-processed, this may be different.
    int32_t lastOffset = tbuf[j8 + tbuf_ncols * (used_nrows - 1)].str32.offset;
    size_t sz = static_cast<size_t>(abs(lastOffset) - 1);

    WritableBuffer* wb = columns[i].strdata;
    size_t write_at = wb->prep_write(sz, strbufs[k].mbuf.rptr());
    strbufs[k].ptr = write_at;
    strbufs[k].sz = sz;
    if (columns[i].type == PT::Str32 && write_at + sz > 0x80000000) {
      dt::shared_lock lock(shmutex, /* exclusive = */ true);
      columns[i].convert_to_str64();
      types[i] = PT::Str64;
      if (verbose) {
        freader.fo.str64_bump(i, columns[i]);
      }
    }
  }
}


void FreadLocalParseContext::push_buffers() {
  // If the buffer is empty, then there's nothing to do...
  if (!used_nrows) return;
  dt::shared_lock lock(shmutex);

  double t0 = verbose? wallclock() : 0;
  size_t ncols = columns.size();
  for (size_t i = 0, j = 0, k = 0; i < ncols; i++) {
    GReaderColumn& col = columns[i];
    if (!col.presentInBuffer) continue;
    void* data = col.data_w();
    int8_t elemsize = static_cast<int8_t>(col.elemsize());

    if (col.typeBumped) {
      // do nothing: the column was not properly allocated for its type, so
      // any attempt to write the data may fail with data corruption
    } else if (col.isstring()) {
      WritableBuffer* wb = col.strdata;
      StrBuf& sb = strbufs[k];
      size_t ptr = sb.ptr;
      size_t sz = sb.sz;
      field64* lo = tbuf + sb.idx8;

      wb->write_at(ptr, sz, sb.mbuf.rptr());

      if (elemsize == 4) {
        int32_t* dest = static_cast<int32_t*>(data) + row0 + 1;
        int32_t iptr = static_cast<int32_t>(ptr);
        for (size_t n = 0; n < used_nrows; ++n) {
          int32_t soff = lo->str32.offset;
          *dest++ = (soff < 0)? soff - iptr : soff + iptr;
          lo += tbuf_ncols;
        }
      } else {
        int64_t* dest = static_cast<int64_t*>(data) + row0 + 1;
        int64_t iptr = static_cast<int64_t>(ptr);
        for (size_t n = 0; n < used_nrows; ++n) {
          int64_t soff = lo->str32.offset;
          *dest++ = (soff < 0)? soff - iptr : soff + iptr;
          lo += tbuf_ncols;
        }
      }
      k++;

    } else {
      const field64* src = tbuf + j;
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




//==============================================================================
// FreadTokenizer
//==============================================================================
void parse_string(FreadTokenizer&);


/**
 * skip_eol() is used to consume a "newline" token from the current parsing
 * location (`ch`). Specifically,
 *   (1) if there is a newline sequence at the current parsing position, this
 *       function advances the parsing position past the newline and returns
 *       true;
 *   (2) otherwise it returns false and the current parsing location remains
 *       unchanged.
 *
 * We recognize the following sequences as newlines (where "LF" is byte 0x0A
 * or '\\n', and "CR" is 0x0D or '\\r'):
 *     CR CR LF
 *     CR LF
 *     LF CR
 *     LF
 *     CR  (only if `LFpresent` is false)
 *
 * Here LF and CR-LF are the most commonly used line endings, while LF-CR and
 * CR are encountered much less frequently. The sequence CR-CR-LF is not
 * usually recognized as a single newline by most text editors. However we find
 * that occasionally a file with CR-LF endings gets recoded into CR-CR-LF line
 * endings by buggy software.
 *
 * In addition, CR (\\r) is treated specially: it is considered a newline only
 * when `LFpresent` is false. This is because it is common to find files created
 * by programs that don't account for '\\r's and fail to quote fields containing
 * these characters. If we were to treat these '\\r's as newlines, the data
 * would be parsed incorrectly. On the other hand, there are files where '\\r's
 * are used as valid newlines. In order to handle both of these cases, we
 * introduce parameter `LFpresent` which is set to true if there is any '\\n'
 * found in the file, in which case a standalone '\\r' will not be considered a
 * newline.
 */
bool FreadTokenizer::skip_eol() {
  // we call eol() when we expect to be at a newline, so optimize as if we are
  // at the end of line.
  if (*ch == '\n') {       // '\n\r' or '\n'
    ch += 1 + (ch[1] == '\r');
    return true;
  }
  if (*ch == '\r') {
    if (ch[1] == '\n') {   // '\r\n'
      ch += 2;
      return true;
    }
    if (ch[1] == '\r' && ch[2] == '\n') {  // '\r\r\n'
      ch += 3;
      return true;
    }
    if (!LFpresent) {      // '\r'
      ch++;
      return true;
    }
  }
  return false;
}


/**
 * Return true iff the tokenizer's current position `ch` is a valid field
 * terminator (either a `sep` or a newline). This does not advance the tokenizer
 * position.
 */
bool FreadTokenizer::end_of_field() {
  // \r is 13, \n is 10, and \0 is 0. The second part is optimized based on the
  // fact that the characters in the ASCII range 0..13 are very rare, so a
  // single check `tch<=13` is almost equivalent to checking whether `tch` is one
  // of \r, \n, \0. We cast to unsigned first because `char` type is signed by
  // default, and therefore characters in the range 0x80-0xFF are negative.
  // We use eol() because that looks at LFpresent inside it w.r.t. \r
  char c = *ch;
  if (c == sep) return true;
  if (static_cast<uint8_t>(c) > 13) return false;
  if (c == '\n' || (c == '\0' && ch == eof)) return true;
  if (c == '\r') {
    if (LFpresent) {
      const char* tch = ch + 1;
      while (*tch == '\r') tch++;
      if (*tch == '\n') return true;
    } else {
      return true;
    }
  }
  return false;
}


const char* FreadTokenizer::end_NA_string(const char* fieldStart) {
  const char* const* nastr = NAstrings;
  const char* mostConsumed = fieldStart;
  while (*nastr) {
    const char* ch1 = fieldStart;
    const char* ch2 = *nastr;
    while (*ch1==*ch2 && *ch2!='\0') { ch1++; ch2++; }
    if (*ch2=='\0' && ch1>mostConsumed) mostConsumed=ch1;
    nastr++;
  }
  return mostConsumed;
}


/**
 * Skip whitespace at the beginning/end of a field.
 *
 * If `sep=' '` (Space), then whitespace shouldn't be skipped at all.
 * If `sep='\\t'` (Tab), then only ' ' characters are considered whitespace.
 * For all other seps we assume that both ' ' and '\\t' characters are
 * whitespace to be skipped.
 */
void FreadTokenizer::skip_whitespace() {
  // skip space so long as sep isn't space and skip tab so long as sep isn't tab
  if (whiteChar == 0) {   // whiteChar==0 means skip both ' ' and '\t';  sep is neither ' ' nor '\t'.
    while (*ch == ' ' || *ch == '\t') ch++;
  } else {
    while (*ch == whiteChar) ch++;  // sep is ' ' or '\t' so just skip the other one.
  }
}


/**
 * Skip whitespace at the beginning of a line. This whitespace does not count
 * as a separator even if `sep=' '`.
 */
void FreadTokenizer::skip_whitespace_at_line_start() {
  if (sep == '\t') {
    while (*ch == ' ') ch++;
  } else {
    while (*ch == ' ' || *ch == '\t') ch++;
  }
}


/**
 * Compute the number of fields on the current line (taking into account the
 * global `sep`, and `quoteRule`), and move the parsing location to the
 * beginning of the next line.
 * Returns the number of fields on the current line, or -1 if the line cannot
 * be parsed using current settings, or 0 if the line is empty (even though an
 * empty line may be viewed as a single field).
 */
int FreadTokenizer::countfields()
{
  const char* ch0 = ch;
  if (sep==' ') while (*ch==' ') ch++;  // multiple sep==' ' at the start does not mean sep
  skip_whitespace();
  if (skip_eol() || ch==eof) {
    return 0;
  }
  int ncol = 1;
  while (ch < eof) {
    parse_string(*this);
    // Field() leaves *ch resting on sep, eol or eof
    if (*ch == sep) {
      if (sep == ' ') {
        while (*ch == ' ') ch++;
        if (ch == eof || skip_eol()) break;
        ncol++;
        continue;
      } else if (sep != '\n') {
        ch++;
        ncol++;
        continue;
      }
    }
    if (ch == eof || skip_eol()) {
      break;
    }
    ch = ch0;
    return -1;  // -1 means this line not valid for this sep and quote rule
  }
  return ncol;
}



//==============================================================================
// FreadObserver
//==============================================================================

FreadObserver::FreadObserver(const GenericReader& g_) : g(g_) {
  t_start = wallclock();
  t_initialized = 0;
  t_parse_parameters_detected = 0;
  t_column_types_detected = 0;
  t_frame_allocated = 0;
  t_data_read = 0;
  t_data_reread = 0;
  time_read_data = 0;
  time_push_data = 0;
  input_size = 0;
  n_rows_read = 0;
  n_cols_read = 0;
  n_lines_sampled = 0;
  n_rows_allocated = 0;
  n_cols_allocated = 0;
  n_cols_reread = 0;
  allocation_size = 0;
  read_data_nthreads = 0;
}

FreadObserver::~FreadObserver() {}


void FreadObserver::report() {
  double t_end = wallclock();
  xassert(t_start <= t_initialized &&
          t_initialized <= t_parse_parameters_detected &&
          t_parse_parameters_detected <= t_column_types_detected &&
          t_column_types_detected <= t_frame_allocated &&
          t_frame_allocated <= t_data_read &&
          t_data_read <= t_data_reread &&
          t_data_reread <= t_end &&
          read_data_nthreads > 0);
  double total_time = std::max(t_end - t_start, 1e-6);
  int    total_minutes = static_cast<int>(total_time/60);
  double total_seconds = total_time - total_minutes * 60;
  double params_time = t_parse_parameters_detected - t_initialized;
  double types_time = t_column_types_detected - t_parse_parameters_detected;
  double alloc_time = t_frame_allocated - t_column_types_detected;
  double read_time = t_data_read - t_frame_allocated;
  double reread_time = t_data_reread - t_data_read;
  double makedt_time = t_end - t_data_reread;
  time_read_data /= read_data_nthreads;
  time_push_data /= read_data_nthreads;
  double time_wait_data = read_time + reread_time
                          - time_read_data - time_push_data;
  int p = total_time < 10 ? 5 :
          total_time < 100 ? 6 :
          total_time < 1000 ? 7 : 8;

  g.trace("=============================");
  g.trace("Read %s row%s x %s column%s from %s input in %02d:%06.3fs",
          humanize_number(n_rows_read), (n_rows_read == 1 ? "" : "s"),
          humanize_number(n_cols_read), (n_cols_read == 1 ? "" : "s"),
          filesize_to_str(input_size),
          total_minutes, total_seconds);
  g.trace(" = %*.3fs (%2.0f%%) detecting parse parameters", p,
          params_time, 100 * params_time / total_time);
  g.trace(" + %*.3fs (%2.0f%%) detecting column types using %s sample rows", p,
          types_time, 100 * types_time / total_time,
          humanize_number(n_lines_sampled));
  g.trace(" + %*.3fs (%2.0f%%) allocating [%s x %s] frame (%s) of which "
          "%s (%.0f%%) rows used", p,
          alloc_time, 100 * alloc_time / total_time,
          humanize_number(n_rows_allocated),
          humanize_number(n_cols_allocated),
          filesize_to_str(allocation_size),
          humanize_number(n_rows_read),
          100.0 * n_rows_read / n_rows_allocated);  // may be > 100%
  g.trace(" + %*.3fs (%2.0f%%) reading data using %zu thread%s", p,
          read_time, 100 * read_time / total_time,
          read_data_nthreads, (read_data_nthreads == 1? "" : "s"));
  if (n_cols_reread) {
    g.trace(" + %*.3fs (%2.0f%%) Rereading %d columns due to out-of-sample "
            "type exceptions", p,
            reread_time, 100 * reread_time / total_time,
            n_cols_reread);
  }
  g.trace("    = %*.3fs (%2.0f%%) reading into row-major buffers", p,
          time_read_data, 100 * time_read_data / total_time);
  g.trace("    + %*.3fs (%2.0f%%) saving into the output frame", p,
          time_push_data, 100 * time_push_data / total_time);
  g.trace("    + %*.3fs (%2.0f%%) waiting", p,
          time_wait_data, 100 * time_wait_data / total_time);
  g.trace(" + %*.3fs (%2.0f%%) creating the final Frame", p,
          makedt_time, 100 * makedt_time / total_time);
  if (!messages.empty()) {
    g.trace("=============================");
    for (std::string msg : messages) {
      g.trace("%s", msg.data());
    }
  }
}


void FreadObserver::type_bump_info(
  size_t icol, const GReaderColumn& col, PT new_type,
  const char* field, int64_t len, int64_t lineno)
{
  static const int BUF_SIZE = 1000;
  char temp[BUF_SIZE + 1];
  int n = snprintf(temp, BUF_SIZE,
    "Column %zu (%s) bumped from %s to %s due to <<%.*s>> on row %llu",
    icol, col.repr_name(g),
    ParserLibrary::info(col.type).cname(),
    ParserLibrary::info(new_type).cname(),
    static_cast<int>(len), field, lineno);
  n = std::min(n, BUF_SIZE);
  messages.push_back(std::string(temp, static_cast<size_t>(n)));
}


void FreadObserver::str64_bump(size_t icol, const GReaderColumn& col) {
  static const int BUF_SIZE = 1000;
  char temp[BUF_SIZE + 1];
  int n = snprintf(temp, BUF_SIZE,
    "Column %zu (%s) switched from Str32 to Str64 because its size "
    "exceeded 2GB",
    icol, col.repr_name(g));
  n = std::min(n, BUF_SIZE);
  messages.push_back(std::string(temp, static_cast<size_t>(n)));
}
