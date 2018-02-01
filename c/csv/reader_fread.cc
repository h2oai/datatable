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

static const SType colType_to_stype[NUMTYPE] = {
  ST_VOID,
  ST_BOOLEAN_I1,
  ST_BOOLEAN_I1,
  ST_BOOLEAN_I1,
  ST_BOOLEAN_I1,
  ST_INTEGER_I4,
  ST_INTEGER_I8,
  ST_REAL_F4,
  ST_REAL_F8,
  ST_REAL_F8,
  ST_REAL_F8,
  ST_STRING_I4_VCHAR,
};



//------------------------------------------------------------------------------
// Initialization
//------------------------------------------------------------------------------

FreadReader::FreadReader(GenericReader& greader) : g(greader) {
  targetdir = nullptr;
  // strbufs = nullptr;
  eof = nullptr;
  // nstrcols = 0;
  // ndigits = 0;
  whiteChar = dec = sep = quote = '\0';
  quoteRule = -1;
  stripWhite = false;
  blank_is_a_NAstring = false;
  LFpresent = false;
}

FreadReader::~FreadReader() {
  // free(strbufs);
}


FieldParseContext FreadReader::makeFieldParseContext(
    const char*& ch, field64* target, const char* anchor
) {
  return {
    .ch = ch,
    .target = target,
    .anchor = anchor,
    .eof = eof,
    .NAstrings = g.na_strings,
    .whiteChar = whiteChar,
    .dec = dec,
    .sep = sep,
    .quote = quote,
    .quoteRule = quoteRule,
    .stripWhite = stripWhite,
    .blank_is_a_NAstring = blank_is_a_NAstring,
    .LFpresent = LFpresent,
  };
}



//------------------------------------------------------------------------------
// Detect the separator / quoting rule
//------------------------------------------------------------------------------
class HypothesisNoQC;
class HypothesisQC;
class HypothesisPool;

class Hypothesis {
  protected:
    FieldParseContext& ctx;
    size_t nlines;
    bool invalid;
    int64_t : 56;

  public:
    Hypothesis(FieldParseContext& c) : ctx(c), nlines(0), invalid(false) {}
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
    HypothesisQC(FieldParseContext& c, char q, HypothesisNoQC* p)
      : Hypothesis(c), parent(p), qc(q) {}
    void parse_next_line(HypothesisPool&) override {

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
    HypothesisNoQC(FieldParseContext& ctx)
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
          chfreq['s'] += (c == ' ' && nspaces == 0) - (c == 's');
          if (c == '\n' || c == '\r') {
            if (ctx.skip_eol()) {
              chfreq[' '] -= nspaces;
              chfreq['s'] -= (nspaces > 0);
              break;
            }
          }
        }
        nspaces = (c == ' ')? nspaces + 1 : 0;
        ch++;
      }
      if (!doubleQuoteSeen && chfreq['"']) {
        hp.push_back(new HypothesisQC(ctx, '"', this));
        doubleQuoteSeen = true;
      }
      if (!singleQuoteSeen && chfreq['\'']) {
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
void FreadReader::detect_sep(FieldParseContext&) {
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
void FreadReader::parse_column_names(FieldParseContext& ctx) {
  const char*& ch = ctx.ch;

  // Skip whitespace at the beginning of a line.
  if (stripWhite && (*ch == ' ' || (*ch == '\t' && sep != '\t'))) {
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
    size_t length = static_cast<size_t>(ctx.target->str32.length);

    if (i >= ncols) {
      columns.push_back(GReaderColumn());
    }
    if (length > 0) {
      const uint8_t* usrc = reinterpret_cast<const uint8_t*>(start);
      int res = check_escaped_string(usrc, length, echar);
      if (res == 0) {
        columns[i].name = std::string(start, length);
      } else {
        char* newsrc = new char[length * 4];
        uint8_t* unewsrc = reinterpret_cast<uint8_t*>(newsrc);
        int newlen;
        if (res == 1) {
          newlen = decode_escaped_csv_string(usrc, length, unewsrc, echar);
        } else {
          newlen = decode_win1252(usrc, length, unewsrc);
          newlen = decode_escaped_csv_string(unewsrc, newlen, unewsrc, echar);
        }
        assert(newlen > 0);
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


void FreadReader::userOverride()
{
  size_t ncols = columns.size();
  Py_ssize_t sncols = static_cast<Py_ssize_t>(ncols);
  PyObject* colNamesList = PyList_New(sncols);
  PyObject* colTypesList = PyList_New(sncols);
  for (size_t i = 0; i < ncols; i++) {
    const char* src = columns[i].name.data();
    size_t len = columns[i].name.size();
    Py_ssize_t slen = static_cast<Py_ssize_t>(len);
    PyObject* pycol = slen > 0? PyUnicode_FromStringAndSize(src, slen)
                              : PyUnicode_FromFormat("V%d", i);
    PyObject* pytype = PyLong_FromLong(columns[i].type);
    PyList_SET_ITEM(colNamesList, i, pycol);
    PyList_SET_ITEM(colTypesList, i, pytype);
  }

  g.pyreader().invoke("_override_columns", "(OO)", colNamesList, colTypesList);

  for (size_t i = 0; i < ncols; i++) {
    PyObject* t = PyList_GET_ITEM(colTypesList, i);
    columns[i].type = (int8_t) PyLong_AsUnsignedLongMask(t);
  }
  pyfree(colTypesList);
  pyfree(colNamesList);
}


void FreadReader::progress(double percent/*[0,100]*/) {
  g.pyreader().invoke("_progress", "(d)", percent);
}


DataTablePtr FreadReader::makeDatatable() {
  Column** ccols = NULL;
  size_t ncols = columns.size();
  size_t ocols = columns.nOutputs();
  ccols = (Column**) malloc((ocols + 1) * sizeof(Column*));
  ccols[ocols] = NULL;
  for (size_t i = 0, j = 0; i < ncols; ++i) {
    GReaderColumn& col = columns[i];
    if (!col.presentInOutput) continue;
    SType stype = colType_to_stype[col.type];
    MemoryBuffer* databuf = col.extract_databuf();
    MemoryBuffer* strbuf = col.extract_strbuf();
    ccols[j] = Column::new_mbuf_column(stype, databuf, strbuf);
    j++;
  }
  return DataTablePtr(new DataTable(ccols));
}



//------------------------------------------------------------------------------
// FreadLocalParseContext
//------------------------------------------------------------------------------

FreadLocalParseContext::FreadLocalParseContext(
    size_t bcols, size_t brows, FreadReader& f
  ) : LocalParseContext(bcols, brows), columns(f.columns)
{
  anchor = nullptr;
  quote = f.quote;
  quoteRule = f.quoteRule;
  size_t ncols = columns.size();
  for (size_t i = 0, j = 0; i < ncols; ++i) {
    GReaderColumn& col = columns[i];
    if (!col.presentInBuffer) continue;
    if (col.type == CT_STRING && !col.typeBumped) {
      strbufs.push_back(StrBuf(4096, j, i));
    }
    ++j;
  }
}

FreadLocalParseContext::~FreadLocalParseContext() {}


const char* FreadLocalParseContext::read_chunk(const char*, const char*) {
  return nullptr;
}


void FreadLocalParseContext::postprocess() {
  const uint8_t* zanchor = reinterpret_cast<const uint8_t*>(anchor);
  uint8_t echar = quoteRule == 0? static_cast<uint8_t>(quote) :
                  quoteRule == 1? '\\' : 0xFF;
  size_t nstrcols = strbufs.size();
  for (size_t k = 0; k < nstrcols; ++k) {
    MemoryBuffer* strdest = strbufs[k].mbuf;
    field64* lo = tbuf + strbufs[k].idx8;
    int32_t off = 1;
    size_t bufsize = strbufs[k].mbuf->size();
    for (size_t n = 0; n < used_nrows; n++) {
      int32_t len = lo->str32.length;
      if (len > 0) {
        size_t zlen = static_cast<size_t>(len);
        size_t zoff = static_cast<size_t>(off);
        if (bufsize < zlen * 3 + zoff) {
          bufsize = bufsize * 2 + zlen * 3;
          strdest->resize(bufsize);
        }
        const uint8_t* src = zanchor + lo->str32.offset;
        uint8_t* dest = static_cast<uint8_t*>(strdest->at(off - 1));
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
          assert(newlen > 0);
          newlen = decode_escaped_csv_string(dest, newlen, dest, echar);
          off += static_cast<size_t>(newlen);
          lo->str32.offset = off;
        }
      } else if (len == 0) {
        lo->str32.offset = off;
      } else {
        assert(lo->str32.isna());
        lo->str32.offset = -off;
      }
      lo += tbuf_ncols;
    }
    strbufs[k].ptr = static_cast<size_t>(off - 1);
  }
}


void FreadLocalParseContext::orderBuffer() {
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
    size_t write_at = wb->prep_write(sz, strbufs[k].mbuf->get());
    strbufs[k].ptr = write_at;
  }
}


void FreadLocalParseContext::push_buffers() {
  size_t ncols = columns.size();
  for (size_t i = 0, j = 0, k = 0; i < ncols; i++) {
    const GReaderColumn& col = columns[i];
    if (!col.presentInBuffer) continue;
    void* data = col.data();

    if (col.typeBumped) {
      // do nothing: the column was not properly allocated for its type, so
      // any attempt to write the data may fail with data corruption
    } else if (col.type == CT_STRING) {
      WritableBuffer* wb = col.strdata;
      size_t ptr = strbufs[k].ptr;
      field64* lo = tbuf + strbufs[k].idx8;
      int32_t lastOffset = lo[(used_nrows - 1) * tbuf_ncols].str32.offset;
      size_t sz = static_cast<size_t>(abs(lastOffset)) - 1;

      wb->write_at(ptr, sz, strbufs[k].mbuf->get());

      int32_t* dest = static_cast<int32_t*>(data) + row0 + 1;
      int32_t iptr = (int32_t) ptr;
      for (size_t n = 0; n < used_nrows; n++) {
        int32_t soff = lo->str32.offset;
        *dest++ = (soff < 0)? soff - iptr : soff + iptr;
        lo += tbuf_ncols;
      }
      k++;

    } else {
      int8_t elemsize = typeSize[col.type];
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
}



//==============================================================================
// FieldParseContext
//==============================================================================
void parse_string(FieldParseContext&);


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
bool FieldParseContext::skip_eol() {
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
 * Return True iff `ch` is a valid field terminator character: either a field
 * separator or a newline.
 */
bool FieldParseContext::end_of_field() {
  // \r is 13, \n is 10, and \0 is 0. The second part is optimized based on the
  // fact that the characters in the ASCII range 0..13 are very rare, so a
  // single check `tch<=13` is almost equivalent to checking whether `tch` is one
  // of \r, \n, \0. We cast to unsigned first because `char` type is signed by
  // default, and therefore characters in the range 0x80-0xFF are negative.
  // We use eol() because that looks at LFpresent inside it w.r.t. \r
  char c = *ch;
  if (c == sep) return true;
  if (static_cast<uint8_t>(c) > 13) return false;
  if (c == '\n' || c == '\0') return true;
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


const char* FieldParseContext::end_NA_string(const char* fieldStart) {
  const char* const* nastr = NAstrings;
  const char* mostConsumed = fieldStart; // tests 1550* includes both 'na' and 'nan' in nastrings. Don't stop after 'na' if 'nan' can be consumed too.
  while (*nastr) {
    const char* ch1 = fieldStart;
    const char* ch2 = *nastr;
    while (*ch1==*ch2 && *ch2!='\0') { ch1++; ch2++; }
    if (*ch2=='\0' && ch1>mostConsumed) mostConsumed=ch1;
    nastr++;
  }
  return mostConsumed;
}


void FieldParseContext::skip_white() {
  // skip space so long as sep isn't space and skip tab so long as sep isn't tab
  if (whiteChar == 0) {   // whiteChar==0 means skip both ' ' and '\t';  sep is neither ' ' nor '\t'.
    while (*ch == ' ' || *ch == '\t') ch++;
  } else {
    while (*ch == whiteChar) ch++;  // sep is ' ' or '\t' so just skip the other one.
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
int FieldParseContext::countfields()
{
  const char* ch0 = ch;
  if (sep==' ') while (*ch==' ') ch++;  // multiple sep==' ' at the start does not mean sep
  skip_white();
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


bool FieldParseContext::nextGoodLine(int ncol, bool fill, bool skip_blank_lines) {
  const char* ch0 = ch;
  // we may have landed inside quoted field containing embedded sep and/or embedded \n
  // find next \n and see if 5 good lines follow. If not try next \n, and so on, until we find the real \n
  // We don't know which line number this is, either, because we jumped straight to it. So return true/false for
  // the line number and error message to be worked out up there.
  int attempts = 0;
  while (ch < eof && attempts++<30) {
    while (*ch!='\0' && *ch!='\n' && *ch!='\r') ch++;
    if (*ch=='\0') return false;
    skip_eol();  // move to the first byte of the next line
    int i = 0;
    // countfields() below moves the parse location, so store it in `ch1` in
    // order to revert to the current parsing location later.
    const char* ch1 = ch;
    for (; i < 5; ++i) {
      int n = countfields();  // advances `ch` to the beginning of the next line
      if (n != ncol &&
          !(ncol == 1 && n == 0) &&
          !(skip_blank_lines && n == 0) &&
          !(fill && n < ncol)) break;
    }
    ch = ch1;
    // `i` is the count of consecutive consistent rows
    if (i == 5) break;
  }
  if (*ch!='\0' && attempts<30) return true;
  ch = ch0;
  return false;
}
