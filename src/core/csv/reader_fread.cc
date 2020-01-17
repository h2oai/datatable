//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/reader_fread.h"    // FreadReader
#include "read/fread/fread_tokenizer.h"  // dt::read::FreadTokenizer
#include "utils/misc.h"          // wallclock
#include "py_encodings.h"        // decode_win1252, check_escaped_string, ...



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
  reread_scheduled = false;
  whiteChar = '\0';
  quoteRule = -1;
  cr_is_newline = true;
  fo.input_size = input_size;
}

FreadReader::~FreadReader() {}


dt::read::FreadTokenizer FreadReader::makeTokenizer(
    dt::read::field64* target, const char* anchor) const
{
  dt::read::FreadTokenizer res;
  res.ch = nullptr;
  res.target = target;
  res.anchor = anchor;
  res.eof = eof;
  res.NAstrings = na_strings;
  res.whiteChar = whiteChar;
  res.dec = dec;
  res.sep = sep;
  res.quote = quote;
  res.quoteRule = quoteRule;
  res.strip_whitespace = strip_whitespace;
  res.blank_is_na = blank_is_na;
  res.cr_is_newline = cr_is_newline;

  return res;
}



//------------------------------------------------------------------------------
// Detect the separator / quoting rule
//
// This entire section is WIP
//------------------------------------------------------------------------------
/*
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
*/

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
void FreadReader::detect_sep(dt::read::FreadTokenizer&) {
}

/**
 * [2] Auto detect separator, quoting rule, first line and ncols, simply,
 *     using jump 0 only.
 */
void FreadReader::detect_sep_and_qr() {
  if (verbose) trace("[2] Detect separator, quoting rule, and ncolumns");
  // at each of the 100 jumps how many lines to guess column types (10,000 sample lines)
  constexpr int JUMPLINES = 100;

  int nseps;
  char seps[] = ",|;\t ";  // default seps in order of preference. See ?fread.
  char topSep;             // which sep matches the input best so far
  // using seps[] not *seps for writeability (http://stackoverflow.com/a/164258/403310)

  if (sep == '\xFF') {   // '\xFF' means 'auto'
    nseps = static_cast<int>(strlen(seps));
    topSep = '\xFE';     // '\xFE' means single-column mode
  } else {
    // Cannot use '\n' as a separator, because it prevents us from proper
    // detection of line endings
    if (sep == '\n') sep = '\xFE';
    seps[0] = sep;
    seps[1] = '\0';
    topSep = sep;
    nseps = 1;
    trace("Using supplied sep '%s'",
          sep=='\t' ? "\\t" : sep=='\xFE' ? "\\n" : seps);
  }

  const char* firstJumpEnd = nullptr; // remember where the winning jumpline from jump 0 ends, to know its size excluding header
  int topNumLines = 0;      // the most number of lines with the same number of fields, so far
  int topNumFields = 0;     // how many fields that was, to resolve ties
  int8_t topQuoteRule = -1;  // which quote rule that was
  int topNmax=1;            // for that sep and quote rule, what was the max number of columns (just for fill=true)
                            //   (when fill=true, the max is usually the header row and is the longest but there are more
                            //    lines of fewer)

  dt::read::field64 trash;
  dt::read::FreadTokenizer ctx = makeTokenizer(&trash, nullptr);
  const char*& tch = ctx.ch;

  // We will scan the input line-by-line (at most `JUMPLINES + 1` lines; "+1"
  // covers the header row, at this stage we don't know if it's present), and
  // detect the number of fields on each line. If several consecutive lines
  // have the same number of fields, we'll call them a "contiguous group of
  // lines". Arrays `numFields` and `numLines` contain information about each
  // contiguous group of lines encountered while scanning the first JUMPLINES
  // + 1 lines: 'numFields` gives the count of fields in each group, and
  // `numLines` has the number of lines in each group.
  int numFields[JUMPLINES+1];
  int numLines[JUMPLINES+1];
  for (quoteRule=0; quoteRule<4; quoteRule++) {  // quote rule in order of preference
    for (int s=0; s<nseps; s++) {
      sep = seps[s];
      whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));  // 0 means both ' ' and '\t' to be skipped
      ctx.ch = sof;
      ctx.sep = sep;
      ctx.whiteChar = whiteChar;
      ctx.quoteRule = quoteRule;
      // if (verbose) trace("  Trying sep='%c' with quoteRule %d ...\n", sep, quoteRule);
      for (int i=0; i<=JUMPLINES; i++) { numFields[i]=0; numLines[i]=0; } // clear VLAs
      int i=-1; // The slot we're counting the currently contiguous consistent ncols
      int thisLine=0, lastncol=-1;
      while (tch < eof && thisLine++ < JUMPLINES) {
        // Compute num columns and move `tch` to the start of next line
        int thisncol = ctx.countfields();
        if (thisncol < 0) {
          // invalid file with this sep and quote rule; abort
          numFields[0] = -1;
          break;
        }
        if (thisncol != lastncol) {  // new contiguous consistent ncols started
          numFields[++i] = thisncol;
          lastncol = thisncol;
        }
        numLines[i]++;
      }
      if (numFields[0] == -1) continue;
      if (firstJumpEnd == nullptr) firstJumpEnd = tch;  // if this wins (doesn't get updated), it'll be single column input
      if (topQuoteRule < 0) topQuoteRule = quoteRule;
      bool updated = false;
      int nmax = 0;

      i = -1;
      while (numLines[++i]) {
        if (numFields[i] > nmax) {  // for fill=true to know max number of columns
          nmax = numFields[i];
        }
        if ( numFields[i]>1 &&
            (numLines[i]>1 || (/*blank line after single line*/numFields[i+1]==0)) &&
            ((numLines[i]>topNumLines) ||   // most number of consistent ncols wins
             (numLines[i]==topNumLines && numFields[i]>topNumFields && sep!=topSep && sep!=' '))) {
             //                                       ^ ties in numLines resolved by numFields (more fields win)
             //                                                           ^ but don't resolve a tie with a higher quote
             //                                                             rule unless the sep is different too: #2404, #2839
          topNumLines = numLines[i];
          topNumFields = numFields[i];
          topSep = sep;
          topQuoteRule = quoteRule;
          topNmax = nmax;
          firstJumpEnd = tch;  // So that after the header we know how many bytes jump point 0 is
          updated = true;
          // Two updates can happen for the same sep and quoteRule (e.g. issue_1113_fread.txt where sep=' ') so the
          // updated flag is just to print once.
        } else if (topNumFields == 0 && nseps == 1 && quoteRule != 2) {
          topNumFields = numFields[i];
          topSep = sep;
          topQuoteRule = quoteRule;
          topNmax = nmax;
        }
      }
      if (verbose && updated) {
        trace(sep<' '? "sep='\\x%02x' with %d lines of %d fields using quote rule %d" :
                       "sep='%c' with %d lines of %d fields using quote rule %d",
              sep, topNumLines, topNumFields, topQuoteRule);
      }
    }
  }
  if (!topNumFields) topNumFields = 1;
  xassert(firstJumpEnd && topQuoteRule >= 0);
  quoteRule = ctx.quoteRule = topQuoteRule;
  sep = ctx.sep = topSep;
  whiteChar = ctx.whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));
  if (sep==' ' && !fill) {
    trace("sep=' ' detected, setting fill to True");
    fill = 1;
  }

  int ncols = fill? topNmax : topNumFields;
  xassert(ncols >= 1 && line >= 1);

  // Create vector of Column objects
  columns.add_columns(static_cast<size_t>(ncols));

  first_jump_size = static_cast<size_t>(firstJumpEnd - sof);

  if (verbose) {
    trace("Detected %d columns", ncols);
    if (sep == '\xFE') trace("sep = <single-column mode>");
    else if (sep >= ' ') trace("sep = '%c'", sep);
    else trace("sep = '\\x%02x'", int(sep));
    trace("Quote rule = %d", quoteRule);
    fo.t_parse_parameters_detected = wallclock();
  }
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
    dt::read::FreadTokenizer fctx;
    size_t nchunks;
    size_t chunk_distance;
    const char* last_row_end;

    ColumnTypeDetectionChunkster(FreadReader& fr, dt::read::FreadTokenizer& ft)
        : f(fr), fctx(ft)
    {
      nchunks = 0;
      chunk_distance = 0;
      last_row_end = f.sof;
      determine_chunking_strategy();
    }

    void determine_chunking_strategy() {
      size_t chunk0_size = f.first_jump_size;
      size_t input_size = static_cast<size_t>(f.eof - f.sof);
      if (f.max_nrows < std::numeric_limits<size_t>::max()) {
        nchunks = 1;
        f.trace("Number of sampling jump points = 1 because max_nrows "
                "parameter is used");
      } else if (chunk0_size == 0 || chunk0_size == input_size) {
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

    dt::read::ChunkCoordinates compute_chunk_boundaries(size_t j) {
      dt::read::ChunkCoordinates cc(f.eof, f.eof);
      if (j == 0) {
        if (f.header == 0) {
          cc.set_start_exact(f.sof);
        } else {
          // If `header` is either True or <auto>, we skip the first row during
          // type detection.
          fctx.ch = f.sof;
          int n = fctx.countfields();
          if (n >= 0) cc.set_start_exact(fctx.ch);
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
          bool ok = fctx.next_good_line_start(cc, static_cast<int>(f.get_ncols()),
                                              f.fill, f.skip_blank_lines);
          if (ok) cc.set_start_approximate(fctx.ch);
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
int64_t FreadReader::parse_single_line(dt::read::FreadTokenizer& fctx)
{
  const char*& tch = fctx.ch;

  // detect blank lines
  fctx.skip_whitespace_at_line_start();
  if (tch == eof || fctx.skip_eol()) return 0;

  size_t ncols = columns.size();
  size_t j = 0;
  dt::read::Column dummy_col;
  dummy_col.force_ptype(PT::Str32);

  while (true) {
    dt::read::Column& col = j < ncols ? columns[j] : dummy_col;
    fctx.skip_whitespace();

    const char* fieldStart = tch;
    auto ptype_iter = col.get_ptype_iterator(&fctx.quoteRule);
    while (true) {
      // Try to parse using the regular field parser
      tch = fieldStart;
      parsers[*ptype_iter](fctx);
      fctx.skip_whitespace();
      if (fctx.at_end_of_field()) break;

      // Try to parse as NA
      // TODO: this API is awkward; better have smth like `fctx.parse_na();`
      tch = fctx.end_NA_string(fieldStart);
      fctx.skip_whitespace();
      if (fctx.at_end_of_field()) break;

      if (ParserLibrary::info(*ptype_iter).isstring()) {
        // Do not bump the quote rule, since we cannot be sure that the jump
        // was reliable. Instead, we'll defer quote rule bumping to regular
        // file reading.
        return -1;
      }

      // Try to parse as quoted field
      if (*fieldStart == quote) {
        tch = fieldStart + 1;
        parsers[*ptype_iter](fctx);
        if (*tch == quote) {
          tch++;
          fctx.skip_whitespace();
          if (fctx.at_end_of_field()) break;
        }
      }

      // Finally, bump the column's type and try again
      ++ptype_iter;
    }
    if (j < ncols && ptype_iter.has_incremented()) {
      col.set_ptype(ptype_iter);
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

  dt::read::field64 tmp;
  dt::read::FreadTokenizer fctx = makeTokenizer(&tmp, nullptr);
  const char*& tch = fctx.ch;

  ColumnTypeDetectionChunkster chunkster(*this, fctx);
  size_t nChunks = chunkster.nchunks;

  double sumLen = 0.0;
  double sumLenSq = 0.0;
  int minLen = INT32_MAX;   // int_max so the first if(thisLen<minLen) is always true; similarly for max
  int maxLen = -1;
  int rows_to_sample = static_cast<int>(std::min<size_t>(max_nrows, 100));

  // Start with all columns having the smallest possible type
  columns.setType(PT::Mu);

  // This variable will store column types at the beginning of each jump
  // so that we can revert to them if the jump proves to be invalid.
  auto saved_types = std::unique_ptr<PT[]>(new PT[ncols]);

  for (size_t j = 0; j < nChunks; ++j) {
    dt::read::ChunkCoordinates cc = chunkster.compute_chunk_boundaries(j);
    tch = cc.get_start();
    if (tch >= eof) continue;

    columns.saveTypes(saved_types);

    for (int i = 0; i < rows_to_sample; ++i) {
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
      int thisLineLen = static_cast<int>(tch - lineStart);
      xassert(thisLineLen >= 0);
      sumLen += thisLineLen;
      sumLenSq += 1.0 * thisLineLen * thisLineLen;
      if (thisLineLen<minLen) minLen = thisLineLen;
      if (thisLineLen>maxLen) maxLen = thisLineLen;
    }
    if (verbose && (j == 0 || j == nChunks - 1 ||
                    !columns.sameTypes(saved_types))) {
      trace("Type codes (jump %03d): %s", j, columns.printTypes());
    }
  }

  detect_header();

  if (verbose) {
    trace("Type codes (final): %s", columns.printTypes());
  }

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
    size_t estnrow = static_cast<size_t>(std::ceil(bytesRead/meanLineLen));  // only used for progress meter and verbose line below
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
            allocnrow, estnrow, static_cast<int>(100.0*allocnrow/estnrow-100.0));
    }
    if (nChunks == 1 && tch == eof) {
      if (header == 1) n_sample_lines--;
      allocnrow = n_sample_lines;
      trace("All rows were sampled since file is small so we know nrows=%zd exactly", allocnrow);
    } else {
      xassert(n_sample_lines <= allocnrow);
    }
    if (max_nrows < allocnrow) {
      trace("Alloc limited to nrows=%zd according to the provided max_nrows argument.", max_nrows);
      allocnrow = max_nrows;
    }
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

  dt::read::field64 tmp;
  dt::read::FreadTokenizer fctx = makeTokenizer(&tmp, nullptr);
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
      columns.add_columns(static_cast<size_t>(ncols_header - sncols));
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
    if (!ParserLibrary::info(header_types[j]).isstring()) {
      all_strings = false;
      break;
    }
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
    // Since the first row is not header, re-parse it again in order to
    // know the column types more precisely.
    tch = sof;
    parse_single_line(fctx);
  }
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

/**
 * This helper method tests whether '\\n' characters are present in the file,
 * and sets the `cr_is_newline` flag accordingly.
 *
 * If '\\n' exists in the file, then `cr_is_newline` is false, and standalone
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
  cr_is_newline = !(ch < eof && *ch == '\n');
  if (cr_is_newline) {
    trace("LF character (\\n) not found in input, "
          "CR character (\\r) will be treated as a newline");
  } else {
    trace("LF character (\\n) found in input, "
          "\\r-only newlines will not be recognized");
  }

}


/**
 * Detect whether the file contains an initial "preamble" section (comments
 * at the top of the file), and if so skip them.
 */
void FreadReader::skip_preamble() {
  if (skip_to_line || !skip_to_string.empty()) {
    // If the user has explicitly requested skip then do not try to detect
    // any other comment section.
    return;
  }

  dt::read::field64 tmp;
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
// TODO name-cleaning should be a method of dt::read::Column
void FreadReader::parse_column_names(dt::read::FreadTokenizer& ctx) {
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
      columns.add_columns(1);
    }
    if (ilen > 0) {
      const uint8_t* usrc = reinterpret_cast<const uint8_t*>(start);
      int res = check_escaped_string(usrc, zlen, echar);
      if (res == 0) {
        columns[i].set_name(std::string(start, zlen));
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
        zlen = static_cast<size_t>(newlen);
        columns[i].set_name(std::string(newsrc, zlen));
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
      columns[j].swap_names(columns[j-1]);
    }
    columns[0].set_name("index");
  }
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
  time_read_data = 0.0;
  time_push_data = 0.0;
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
  double total_time = std::max(t_end - t_start + g.t_open_input, 1e-6);
  int    total_minutes = static_cast<int>(total_time/60);
  double total_seconds = total_time - total_minutes * 60;
  double params_time = t_parse_parameters_detected - t_initialized;
  double types_time = t_column_types_detected - t_parse_parameters_detected;
  double alloc_time = t_frame_allocated - t_column_types_detected;
  double read_time = t_data_read - t_frame_allocated;
  double reread_time = t_data_reread - t_data_read;
  double makedt_time = t_end - t_data_reread;
  double t_read = time_read_data.load() / read_data_nthreads;
  double t_push = time_push_data.load() / read_data_nthreads;
  double time_wait_data = read_time + reread_time - t_read - t_push;
  int p = total_time < 10 ? 5 :
          total_time < 100 ? 6 :
          total_time < 1000 ? 7 : 8;

  g.trace("=============================");
  g.trace("Read %s row%s x %s column%s from %s input in %02d:%06.3fs",
          humanize_number(n_rows_read), (n_rows_read == 1 ? "" : "s"),
          humanize_number(n_cols_read), (n_cols_read == 1 ? "" : "s"),
          filesize_to_str(input_size),
          total_minutes, total_seconds);
  g.trace(" = %*.3fs (%2.0f%%) %s", p,
          g.t_open_input, 100 * g.t_open_input / total_time,
          g.input_is_string? "converting input string into bytes"
                           : "memory-mapping input line");
  g.trace(" + %*.3fs (%2.0f%%) detecting parse parameters", p,
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
          t_read, 100 * t_read / total_time);
  g.trace("    + %*.3fs (%2.0f%%) saving into the output frame", p,
          t_push, 100 * t_push / total_time);
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
  size_t icol, const dt::read::Column& col, PT new_type,
  const char* field, int64_t len, int64_t lineno)
{
  static const int BUF_SIZE = 1000;
  char temp[BUF_SIZE + 1];
  int n = snprintf(temp, BUF_SIZE,
    "Column %zu (%s) bumped from %s to %s due to <<%.*s>> on row %zu",
    icol, col.repr_name(g), col.typeName(),
    ParserLibrary::info(new_type).cname(),
    static_cast<int>(len), field, static_cast<size_t>(lineno));
  n = std::min(n, BUF_SIZE);
  messages.push_back(std::string(temp, static_cast<size_t>(n)));
}
