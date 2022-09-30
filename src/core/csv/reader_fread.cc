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
#include <iomanip>
#include "column.h"
#include "csv/reader_fread.h"    // FreadReader
#include "py_encodings.h"        // decode_win1252, check_escaped_string, ...
#include "read/chunk_coordinates.h"
#include "read/parse_context.h"  // dt::read::ParseContext
#include "read/parsers/ptype_iterator.h"  // dt::read::PTypeIterator
#include "stype.h"
#include "utils/logger.h"
#include "utils/misc.h"          // wallclock
#define D() if (verbose) d()


//------------------------------------------------------------------------------
// Initialization
//------------------------------------------------------------------------------

FreadReader::FreadReader(const dt::read::GenericReader& g)
  : GenericReader(g), parsers(dt::read::parser_functions), fo(g)
{
  size_t input_size = datasize();
  targetdir = nullptr;
  xassert(input_size > 0);

  first_jump_size = 0;
  n_sample_lines = 0;
  reread_scheduled = false;
  whiteChar = '\0';
  quoteRule = -1;
  cr_is_newline = true;
  fo.input_size = input_size;
}

FreadReader::~FreadReader() {}


dt::read::ParseContext FreadReader::makeTokenizer() const
{
  dt::read::ParseContext res;
  res.ch = nullptr;
  res.target = nullptr;
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
    ParseContext& ctx;
    size_t nlines;
    bool invalid;
    int64_t : 56;

  public:
    Hypothesis(ParseContext& c) : ctx(c), nlines(0), invalid(false) {}
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
    HypothesisQC(ParseContext& c, char q, HypothesisNoQC* p)
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
    HypothesisNoQC(ParseContext& ctx)
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
void FreadReader::detect_sep(dt::read::ParseContext&) {
}

/**
 * [2] Auto detect separator, quoting rule, first line and ncols, simply,
 *     using jump 0 only.
 */
void FreadReader::detect_sep_and_qr() {
  auto _ = logger_.section("[2] Detect parse settings");
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
    D() << "Using supplied sep '"
        << (sep=='\t' ? "\\t" : sep=='\xFE' ? "\\n" : seps) << "'";
  }

  const char* firstJumpEnd = nullptr; // remember where the winning jumpline from jump 0 ends, to know its size excluding header
  int topNumLines = 0;      // the most number of lines with the same number of fields, so far
  int topNumFields = 0;     // how many fields that was, to resolve ties
  int topQuoteRule = -1;    // which quote rule that was
  int topNmax=1;            // for that sep and quote rule, what was the max number of columns (just for fill=true)
                            //   (when fill=true, the max is usually the header row and is the longest but there are more
                            //    lines of fewer)

  dt::read::field64 tmp;
  dt::read::ParseContext ctx = makeTokenizer();
  ctx.target = &tmp;
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
  int8_t countInvalidQuoteRules = 0;
  bool setQuoteRuleInvalid;
  for (quoteRule=0; quoteRule<4; quoteRule++) {  // quote rule in order of preference
    setQuoteRuleInvalid = false;
    for (int s=0; s<nseps; s++) {
      sep = seps[s];
      whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));  // 0 means both ' ' and '\t' to be skipped
      ctx.ch = sof;
      ctx.sep = sep;
      ctx.whiteChar = whiteChar;
      ctx.quoteRule = quoteRule;
      for (int i=0; i<=JUMPLINES; i++) { numFields[i]=0; numLines[i]=0; } // clear VLAs
      int i=-1; // The slot we're counting the currently contiguous consistent ncols
      int thisLine=0, lastncol=-1;
      while (tch < eof && thisLine++ < JUMPLINES) {
        // Compute num columns and move `tch` to the start of next line
        int thisncol = ctx.countfields();
        if (thisncol < 0) {
          // invalid file with this sep and quote rule; abort
          numFields[0] = -1;
          if (!setQuoteRuleInvalid && quoteRule <= 1) {
            setQuoteRuleInvalid = true;
            ++countInvalidQuoteRules;
          }
          break;
        }
        if (thisncol != lastncol) {  // new contiguous consistent ncols started
          numFields[++i] = thisncol;
          lastncol = thisncol;
        }
        numLines[i]++;
      }
      if (numFields[0] == -1) continue;
      if (firstJumpEnd == nullptr) {
        firstJumpEnd = tch;  // if this wins (doesn't get updated), it'll be single column input
      }
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
        } else if (topNumFields == 0 && nseps == 1 && quoteRule != 3) {
          topNumFields = numFields[i];
          topSep = sep;
          topQuoteRule = quoteRule;
          topNmax = nmax;
        }
      }
      if (updated) {
        D() << "sep='" << sep << "' with " << topNumLines << " lines of "
            << topNumFields << " fields using quote rule " << topQuoteRule;
      }
    }
    if (countInvalidQuoteRules <= 1 && quoteRule == 1) break; else continue;
  }
  if (!topNumFields) topNumFields = 1;
  xassert(firstJumpEnd && topQuoteRule >= 0);
  quoteRule = ctx.quoteRule = static_cast<int8_t>(topQuoteRule);
  sep = ctx.sep = topSep;
  whiteChar = ctx.whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));
  if (sep==' ' && !fill) {
    D() << "sep=' ' detected, setting fill to True";
    fill = 1;
  }

  size_t ncols = static_cast<size_t>(fill? topNmax : topNumFields);
  xassert(ncols >= 1 && line >= 1);

  // Create vector of Column objects
  preframe.set_ncols(ncols);

  first_jump_size = static_cast<size_t>(firstJumpEnd - sof);

  if (verbose) {
    D() << "Detected " << ncols << " columns";
    D() << "Quote rule = " << static_cast<int>(quoteRule);
    if (sep == '\xFE') { D() << "sep = <single-column mode>"; }
    else               { D() << "sep = '" << sep << "'"; }
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
    dt::read::ParseContext fctx;
    size_t nchunks;
    size_t chunk_distance;
    const char* last_row_end;

    ColumnTypeDetectionChunkster(FreadReader& fr, dt::read::ParseContext& ft)
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
        if (f.verbose) {
          f.d() << "Number of sampling jump points = 1 because max_nrows "
                   "parameter is used";
        }
      } else if (chunk0_size == 0 || chunk0_size == input_size) {
        nchunks = 1;
        if (f.verbose) {
          f.d() << "Number of sampling jump points = 1 because input is less "
                   "than 100 lines";
        }
      } else {
        xassert(chunk0_size < input_size);
        nchunks = chunk0_size * 200 < input_size ? 101 :
                  chunk0_size * 20  < input_size ? 11 : 1;
        if (nchunks > 1) chunk_distance = input_size / (nchunks - 1);
        if (f.verbose) {
          auto r = static_cast<double>(input_size) / static_cast<double>(chunk0_size);
          f.d() << "Number of sampling jump points = " << nchunks
                << " because the first chunk was "
                << dt::log::ff(1, 1, r) << "times smaller than the entire file";
        }
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
        while (tch < f.eof && (*tch == '\n' || *tch == '\r')) tch++;

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
int64_t FreadReader::parse_single_line(dt::read::ParseContext& fctx)
{
  const char*& tch = fctx.ch;

  // detect blank lines
  fctx.skip_whitespace_at_line_start();
  if (tch == eof || fctx.skip_eol()) return 0;

  size_t ncols = preframe.ncols();
  size_t j = 0;
  dt::read::InputColumn dummy_col;
  dummy_col.set_rtype(dt::read::RT::RStr32);

  while (true) {
    auto& col = j < ncols ? preframe.column(j) : dummy_col;
    fctx.skip_whitespace();

    const char* fieldStart = tch;
    auto ptype_iter = dt::read::PTypeIterator(
                          col.get_ptype(), col.get_rtype(), &fctx.quoteRule);
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

      if (dt::read::parser_infos[*ptype_iter].type().is_string()) {
        // Do not bump the quote rule, since we cannot be sure that the jump
        // was reliable. Instead, we'll defer quote rule bumping to regular
        // file reading.
        return -1;
      }

      // Try to parse as quoted field
      if (*fieldStart == quote) {
        tch = fieldStart + 1;
        parsers[*ptype_iter](fctx);
        if (tch < eof && *tch == quote) {
          tch++;
          fctx.skip_whitespace();
          if (fctx.at_end_of_field()) break;
        }
      }

      // Finally, bump the column's type and try again
      ++ptype_iter;
    }
    if (j < ncols && ptype_iter.has_incremented()) {
      col.set_ptype(*ptype_iter);
      col.outcol().set_stype(col.get_stype());
    }
    j++;

    if (tch < eof && *tch == sep) {
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
  auto _ = logger_.section("[3] Detect column types and header");
  size_t ncols = preframe.ncols();
  int64_t sncols = static_cast<int64_t>(ncols);

  dt::read::field64 tmp;
  dt::read::ParseContext fctx = makeTokenizer();
  fctx.target = &tmp;
  const char*& tch = fctx.ch;

  ColumnTypeDetectionChunkster chunkster(*this, fctx);
  size_t nChunks = chunkster.nchunks;

  double sumLen = 0.0;
  double sumLenSq = 0.0;
  int minLen = INT32_MAX;   // int_max so the first if(thisLen<minLen) is always true; similarly for max
  int maxLen = -1;
  int rows_to_sample = static_cast<int>(std::min<size_t>(max_nrows, 100));

  // Start with all columns having the smallest possible type
  preframe.reset_ptypes();

  // This variable will store column types at the beginning of each jump
  // so that we can revert to them if the jump proves to be invalid.
  std::vector<dt::read::PT> saved_types(ncols, dt::read::PT::Void);

  for (size_t j = 0; j < nChunks; ++j) {
    dt::read::ChunkCoordinates cc = chunkster.compute_chunk_boundaries(j);
    tch = cc.get_start();
    if (tch >= eof) continue;

    preframe.save_ptypes(saved_types);

    for (int i = 0; i < rows_to_sample; ++i) {
      if (tch >= eof) break;
      const char* lineStart = tch;
      int64_t incols = parse_single_line(fctx);
      if (incols == 0 && (skip_blank_lines || ncols == 1)) {
        continue;
      }
      // bool eol_found = fctx.skip_eol();
      if (incols == -1 || (incols != sncols && !fill)) {
        D() << "A line with too " << (incols < sncols ? "few" : "many")
            << " fields (" << incols << " out of " << ncols << ") was found "
            << "on line " << i << " of sample jump " << j;
        // Restore column types: it is possible that the chunk start was guessed
        // incorrectly, in which case we don't want the types to be bumped
        // invalidly. This applies to all chunks except the first (for which we
        // know that the start is correct).
        if (j == 0) {
          chunkster.last_row_end = eof;
        } else {
          preframe.set_ptypes(saved_types);
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
                    !preframe.are_same_ptypes(saved_types))) {
      D() << "Type codes (jump " << j << "): " << preframe.print_ptypes();
    }
  }
  D() << "Type codes  (final): " << preframe.print_ptypes();

  detect_header();

  allocnrow = 1;
  meanLineLen = 0;

  if (n_sample_lines == 0) {
    // During type detection the first row is skipped (unless header==0),
    // if n_sample_lines is 0, it means only the header row is present.
    preframe.reset_ptypes();
    allocnrow = 0;
    meanLineLen = sumLen;
  } else {
    double bytesRead = static_cast<double>(eof - sof);
    double nlines = static_cast<double>(n_sample_lines);
    meanLineLen = std::max(sumLen/nlines, 1.0);
    size_t estnrow = static_cast<size_t>(std::ceil(bytesRead/meanLineLen));
    double sd = std::sqrt( (sumLenSq - (sumLen*sumLen)/nlines)/(nlines-1) );
    allocnrow = std::max(static_cast<size_t>(bytesRead / std::max(meanLineLen - 2*sd,
                                                                  static_cast<double>(minLen))),
                         static_cast<size_t>(1.1 * static_cast<double>(estnrow)));
    allocnrow = std::min(allocnrow, 2*estnrow);
    // sd can be very close to 0.0 sometimes, so apply a +10% minimum
    // blank lines have length 1 so for fill=true apply a +100% maximum. It'll be grown if needed.
    if (verbose) {
      d() << "=====";
      d() << "Sampled " << n_sample_lines << " rows at " << nChunks << " jump point(s)";
      d() << "Bytes from first data row to the end of last row: " << bytesRead;
      d() << "Line length: mean=" << dt::log::ff(2, 2, meanLineLen)
          << " sd=" << dt::log::ff(2, 2, sd)
          << " min=" << minLen << " max=" << maxLen;
      d() << "Estimated number of rows: " << estnrow;
      d() << "Initial alloc = " << allocnrow << " rows (using bytes/max(mean-2*sd,min) clamped between [1.1*estn, 2.0*estn])";
    }
    if (nChunks == 1 && tch == eof) {
      if (header == 1) n_sample_lines--;
      allocnrow = n_sample_lines;
      D() << "All rows were sampled since file is small so we know nrows=" << allocnrow << " exactly";
    } else {
      xassert(n_sample_lines <= allocnrow);
    }
    if (max_nrows < allocnrow) {
      D() << "Alloc limited to nrows=" << max_nrows << " according to the provided max_nrows argument";
      allocnrow = max_nrows;
    }
  }
  fo.n_lines_sampled = n_sample_lines;
}


/**
 * Detect whether the first line in input is the header or not.
 */
void FreadReader::detect_header() {
  if (!dt::ISNA<int8_t>(header)) return;
  size_t ncols = preframe.ncols();
  int64_t sncols = static_cast<int64_t>(ncols);

  dt::read::field64 tmp;
  dt::read::ParseContext fctx = makeTokenizer();
  fctx.target = &tmp;
  const char*& tch = fctx.ch;

  // Detect types in the header column
  auto saved_types = preframe.get_ptypes();
  tch = sof;
  preframe.reset_ptypes();
  int64_t ncols_header = parse_single_line(fctx);
  auto header_types = preframe.get_ptypes();
  preframe.set_ptypes(saved_types);

  if (ncols_header != sncols && n_sample_lines > 0 && !fill) {
    header = true;
    D() << "`header` determined to be True because the first line contains "
           "different number of columns (" << ncols_header << ") than the rest "
           "of the file (" << ncols << ")";
    if (ncols_header > sncols) {
      fill = true;
      D() << "Setting `fill` to True because the header contains more columns "
             "than the data";
      preframe.set_ncols(static_cast<size_t>(ncols_header));
    }
    return;
  }

  if (n_sample_lines > 0) {
    for (size_t j = 0; j < ncols; ++j) {
      if (dt::read::parser_infos[header_types[j]].type().is_string() &&
          !dt::read::parser_infos[saved_types[j]].type().is_string() &&
          saved_types[j] != dt::read::PT::Void) {
        header = true;
        D() << "`header` determined to be True due to column " << j + 1
            << " containing a string on row 1 and type "
            << dt::read::parser_infos[saved_types[j]].name()
            << " in the rest of the sample";
        return;
      }
    }
  }

  bool all_strings = true;
  for (size_t j = 0; j < ncols; ++j) {
    if (!dt::read::parser_infos[header_types[j]].type().is_string_or_void()) {
      all_strings = false;
      break;
    }
  }
  if (all_strings) {
    header = true;
    D() << "`header` determined to be `True` because all inputs columns are "
           "strings/voids and better guess is not possible";
  } else {
    header = false;
    D() << "`header` determined to be `False` because some of the fields on "
           "the first row are not of the string/void type";
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
  * This method attempts to detect the value of the `cr_is_newline`
  * parse parameter. When `cr_is_newline` is true, a standalone '\\r'
  * character is treated as a line separator. When the value is false,
  * a standalone '\\r' does not start a new line.
  *
  * Only the first 64Kb of text will be scanned. If any \\n is found,
  * then cr_is_newline will be set to false. However, if at least 10
  * \\r are found without encountering a single \\n, then
  * cr_is_newline will be set to true. If after reading 64Kb of input
  * only \\r's were seen, then cr_is_newline will again be set to
  * true. If neither \\r's nor \\n's were seen, then cr_is_newline
  * will be set to false (i.e. newlines are \\n-based).
  *
  * While scanning, we ignore any newline characters that occur
  * within quoted fields.
  */
void FreadReader::detect_lf() {
  size_t cr_count = 0;
  bool in_quoted_field = false;
  const char* ch = sof;
  const char* end = std::min(eof, sof + 65536);
  for (; ch < end; ++ch) {
    if (in_quoted_field) {
      if (*ch == quote) in_quoted_field = false;
      else if (*ch == '\\') ch++;  // skip the next character
    }
    else if (*ch == '\n') {
      cr_is_newline = false;
      D() << "LF character (\\n) found in input, "
             "\\r-only newlines will be prohibited";
      return;
    }
    else if (*ch == '\r') {
      cr_count++;
      if (cr_count == 10) break;
    }
    else if (*ch == quote) {
      in_quoted_field = true;
    }
  }
  if (cr_count) {
    cr_is_newline = true;
    D() << "Found " << dt::log::plural(cr_count, "\\r character")
        << " and no \\n characters within the first " << ch - sof
        << " bytes of input, \\r will be treated as a newline";
  }
  else {
    cr_is_newline = false;
    D() << "Found no \\r or \\n characters within the first " << ch - sof
        << " bytes of input; default \\n newlines will be assumed";
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
  auto fctx = makeTokenizer();
  fctx.target = &tmp;
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
      if (*ch == '#' || *ch == '%' || *ch == ';') {
        char c = ch+1 < eof? ch[1] : ' ';
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '-' || c == '=' || c == '~' || c == '*') {
          comment_char = *ch;
        }
      }
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
    D() << "Comment section (" << comment_lines << " lines starting with '"
        << comment_char << "') found at the top of the file and skipped";
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
 * not, a `IOError` will be thrown.
 */
// TODO name-cleaning should be a method of dt::read::Column
void FreadReader::parse_column_names(dt::read::ParseContext& ctx) {
  const char*& ch = ctx.ch;

  // Skip whitespace at the beginning of a line.
  if (strip_whitespace && (*ch == ' ' || (*ch == '\t' && sep != '\t'))) {
    while (*ch == ' ' || *ch == '\t') ch++;
  }

  size_t ncols = preframe.ncols();
  size_t ncols_found;
  for (size_t i = 0; ; ++i) {
    // Parse string field, but do not advance `ctx.target`: on the next
    // iteration we will write into the same place.
    dt::read::parse_string(ctx);
    auto start = static_cast<const char*>(ctx.strbuf.rptr())
                 + ctx.target->str32.offset;
    int32_t ilen = ctx.target->str32.length;

    if (i >= ncols) {
      preframe.set_ncols(i + 1);
    }
    if (ilen > 0) {
      preframe.column(i).set_name(std::string(start, start + ilen));
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
      throw IOError() << "Internal error: cannot parse column names";
    }
  }

  if (sep == ' ' && ncols_found == ncols - 1) {
    for (size_t j = ncols - 1; j > 0; j--){
      preframe.column(j).swap_names(preframe.column(j-1));
    }
    preframe.column(0).set_name("index");
  }
}




//==============================================================================
// FreadObserver
//==============================================================================

FreadObserver::FreadObserver(const dt::read::GenericReader& g_) : g(g_) {
  t_start = wallclock();
  t_initialized = 0;
  t_parse_parameters_detected = 0;
  t_column_types_detected = 0;
  t_frame_allocated = 0;
  t_data_read = 0;
  time_read_data = 0.0;
  time_push_data = 0.0;
  input_size = 0;
  n_rows_read = 0;
  n_cols_read = 0;
  n_lines_sampled = 0;
  n_rows_allocated = 0;
  n_cols_allocated = 0;
  allocation_size = 0;
  read_data_nthreads = 0;
}

FreadObserver::~FreadObserver() {}


void FreadObserver::report() {
  double t_end = wallclock();
  xassert(g.verbose);
  xassert(t_start <= t_initialized &&
          t_initialized <= t_parse_parameters_detected &&
          t_parse_parameters_detected <= t_column_types_detected &&
          t_column_types_detected <= t_frame_allocated &&
          t_frame_allocated <= t_data_read &&
          t_data_read <= t_end &&
          read_data_nthreads > 0);
  double total_time = std::max(t_end - t_start + g.t_open_input, 1e-6);
  int    total_minutes = static_cast<int>(total_time/60);
  double total_seconds = total_time - total_minutes * 60;
  double params_time = t_parse_parameters_detected - t_initialized;
  double types_time = t_column_types_detected - t_parse_parameters_detected;
  double alloc_time = t_frame_allocated - t_column_types_detected;
  double read_time = t_data_read - t_frame_allocated;
  double makedt_time = t_end - t_data_read;
  double t_read = time_read_data.load() / static_cast<double>(read_data_nthreads);
  double t_push = time_push_data.load() / static_cast<double>(read_data_nthreads);
  double time_wait_data = read_time - t_read - t_push;
  int p = total_time < 10 ? 5 :
          total_time < 100 ? 6 :
          total_time < 1000 ? 7 : 8;

  using dt::log::ff;
  g.d() << "=============================";
  g.d() << "Read " << humanize_number(n_rows_read) << " rows x "
        << humanize_number(n_cols_read) << " columns from "
        << filesize_to_str(input_size) << " input in "
        << std::setfill('0') << std::setw(2) << total_minutes << ":"
        << ff(6, 3, total_seconds) << "s";
  g.d() << " = " << ff(p, 3, g.t_open_input) << "s ("
        << ff(2, 0, 100 * g.t_open_input / total_time) << "%)"
        << " memory-mapping input file";
  g.d() << " + " << ff(p, 3, params_time) << "s ("
        << ff(2, 0, 100 * params_time / total_time) << "%)"
        << " detecting parse parameters";
  g.d() << " + " << ff(p, 3, types_time) << "s ("
        << ff(2, 0, 100 * types_time / total_time) << "%)"
        << " detecting column types using "
        << humanize_number(n_lines_sampled) << " sample rows";
  g.d() << " + " << ff(p, 3, alloc_time) << "s ("
        << ff(2, 0, 100 * alloc_time / total_time) << "%)"
        << " allocating [" << humanize_number(n_rows_allocated)
        << " x " << humanize_number(n_cols_allocated) << "] frame ("
        << filesize_to_str(allocation_size) << ") of which "
        << humanize_number(n_rows_read) << " ("
        << ff(3, 0, 100.0 * static_cast<double>(n_rows_read) / static_cast<double>(n_rows_allocated))
        << "%) rows used";
  g.d() << " + " << ff(p, 3, read_time) << "s ("
        << ff(2, 0, 100 * read_time / total_time) << "%)"
        << " reading data";
  g.d() << "    = " << ff(p, 3, t_read) << "s (" << ff(2, 0, 100 * t_read / total_time) << "%) reading into row-major buffers";
  g.d() << "    = " << ff(p, 3, t_push) << "s (" << ff(2, 0, 100 * t_push / total_time) << "%) saving into the output frame";
  g.d() << "    = " << ff(p, 3, time_wait_data) << "s (" << ff(2, 0, 100 * time_wait_data / total_time) << "%) waiting";
  g.d() << "    = " << ff(p, 3, makedt_time) << "s (" << ff(2, 0, 100 * makedt_time / total_time) << "%) creating the final Frame";
  if (!messages.empty()) {
    g.d() << "=============================";
    for (const auto& msg : messages) {
      g.d() << msg;
    }
  }
}


void FreadObserver::type_bump_info(
  size_t icol, const dt::read::InputColumn& col, dt::read::PT new_type,
  const char* field, int64_t len, int64_t lineno)
{
  // Maximum number of characters to be printed out for a data sample
  static const size_t MAX_FIELD_LEN = 1000;

  // The actual number of characters for the `field` data
  size_t field_len = std::min(MAX_FIELD_LEN, static_cast<size_t>(len));

  std::stringstream ss;
  ss << "Column " << icol
     << " (" << col.repr_name(g) << ") bumped from " << col.typeName()
     << " to " << dt::read::parser_infos[new_type].name()
     << " due to <<" << std::string(field, field_len) << ">>"
     << " on row " << static_cast<size_t>(lineno);

  messages.push_back(ss.str());
}
