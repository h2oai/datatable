//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include <cmath>  // std::log10
#include <iostream>
#include "buffer.h"
#include "read2/_declarations.h"
#include "utils/assert.h"
namespace dt {
namespace read2 {

static_assert(static_cast<char>(0x7F) > 0 &&
              static_cast<char>(0x80) < 0 &&
              static_cast<char>(0xFF) < 0, "char should be signed");
static_assert('\t' == '\x09' &&
              '\n' == '\x0a' &&
              '\v' == '\x0b' &&
              '\f' == '\x0c' &&
              '\r' == '\x0d', "Invalid ASCII codes");

static int COUNTS_BUFFER[128 * 100];



//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

/**
  * This class's job is to auto-detect various parse settings for a
  * CSV file (see `CsvParseSettings` struct). Some of these settings
  * may be fixed by the user to explicit values, in which case we
  * want to stick to the user's wishes.
  *
  * The way this class works is built upon two observations:
  *
  * 1) If a certain punctuation character, such as '\t', is a
  *    separator then this character will appear the same number of
  *    times in each row (outside of quoted fields). This is assuming
  *    we have a "regular" file with same number of fields in each
  *    row.
  *
  *    Thus, counting occurrences of each character in each row and
  *    then selecting the character that had the most stable count of
  *    appearances in all rows, is a good method of finding the
  *    separator in a single pass.
  *
  * 2) If there is a quoted field in the data, then it must be both
  *    preceded/followed by a separator. This allows us to infer the
  *    separator, or at least significantly narrow the set of possible
  *    alternatives.
  *
  * When applying these heuristics in practice, however, we quickly
  * find ourselves in ambiguous situations. For example, if there is
  * a '\r' character, it may or may not signify a line break. If we
  * encounter a quote '"', it could indicate a beginning of a quoted
  * field, or it may not. And so on.
  *
  * When such ambiguous situations arise, we want to explore all the
  * possibilities, and then choose the one that works best (or at
  * least the one that does not produce an error). This is achieved
  * by creating "alternative hypotheses" -- copies of the
  * `CsvParseSettingsDetector` class that are connected into a linked
  * list.
  *
  * Thus, when an ambiguous parse is encountered, we clone the current
  * class, and resolve a parse parameters differently in the current
  * instance and in the clone. The clone is then inserted into the
  * linked list after the current instance. In the end, we check
  * which instance was able to parse the input without errors, and
  * ultimately use a heuristic to decide which of the several parse
  * instances to choose (for example, separator ',' is considered
  * "better" than any other). Some hypotheses are even marked as
  * "fallback", meaning that we won't even try to use them if the
  * previous hypothesis succeeded.
  *
  */
class CsvParseSettingsDetector {
  private:
    static constexpr int WHITESPACE = 0x57;

    std::unique_ptr<CsvParseSettingsDetector> nextHypothesis_;
    Buffer buffer_;
    const char* ch_;          // current scan position
    const char* eof_;         // end of the data buffer
    const char* sol_;         // current start of line
    const char* chBeforeWhitespace_;
    int* counts_;
    int maxLinesToRead_;
    int nLinesRead_;
    bool moreDataAvailable_;
    bool error_;              // when true, then current parse settings are
                              // incompatible with the input. Meaning that
                              // the current hypothesis should be abandoned.
    bool done_;               // when true, we have parsed all the data
                              // this could be set without reaching eof_,
                              // if there is a blank line in the input.
    bool atStartOfLine_;
    bool atEndOfLine_;
    bool atSeparator_;
    bool isFallbackTheory_;

    SeparatorKind separatorKind_;
    NewlineKind newlineKind_;
    QuoteKind quoteKind_;
    QuoteRule quoteRule_;
    char separatorChar_;
    bool skipBlankLines_;
    bool unevenRows_;         // aka "fill = True"
    int : 16;
    std::string separatorString_;

  public:
    CsvParseSettingsDetector()
      : ch_(nullptr),
        eof_(nullptr),
        sol_(nullptr),
        chBeforeWhitespace_(nullptr),
        counts_(COUNTS_BUFFER),
        maxLinesToRead_(10),
        nLinesRead_(0),
        moreDataAvailable_(false),
        error_(false),
        done_(false),
        atStartOfLine_(false),
        atEndOfLine_(false),
        atSeparator_(false),
        isFallbackTheory_(false),
        separatorKind_(SeparatorKind::AUTO),
        newlineKind_(NewlineKind::AUTO),
        quoteKind_(QuoteKind::AUTO),
        quoteRule_(QuoteRule::AUTO),
        separatorChar_('\xff'),
        skipBlankLines_(false),
        unevenRows_(false) {}

    CsvParseSettingsDetector* setBuffer(Buffer buf) {
      replaceBuffer(buf);
      return this;
    }

    CsvParseSettingsDetector* setQuoteKind(QuoteKind qk) {
      quoteKind_ = qk;
      return this;
    }

    CsvParseSettingsDetector* setSeparatorKind(SeparatorKind sk) {
      separatorKind_ = sk;
      return this;
    }

    CsvParseSettingsDetector* setSeparatorChar(char c) {
      separatorKind_ = SeparatorKind::CHAR;
      separatorChar_ = c;
      return this;
    }

    CsvParseSettingsDetector* setNewlineKind(NewlineKind nk) {
      newlineKind_ = nk;
      return this;
    }


    CsvParseSettingsDetector* detect() {
      parseAll();
      if (error_) {
        return nextHypothesis_? nextHypothesis_->detect() : nullptr;
      }
      if (!nextHypothesis_ || nextHypothesis_->isFallbackTheory_) {
        return this;
      }
      auto alternative = nextHypothesis_->detect();
      return alternative? bestHypothesis(this, alternative) : this;
    }



  private:
    CsvParseSettingsDetector(const CsvParseSettingsDetector& other)
      : buffer_(other.buffer_),
        ch_(other.sol_),
        eof_(other.eof_),  // move the parse pointer to start-of-line
        sol_(other.sol_),
        counts_(other.counts_),
        nLinesRead_(other.nLinesRead_),
        moreDataAvailable_(other.moreDataAvailable_),
        separatorKind_(other.separatorKind_),
        newlineKind_(other.newlineKind_),
        quoteKind_(other.quoteKind_),
        quoteRule_(other.quoteRule_),
        separatorChar_(other.separatorChar_),
        skipBlankLines_(other.skipBlankLines_),
        unevenRows_(other.unevenRows_),
        separatorString_(other.separatorString_) {}

    CsvParseSettingsDetector* newHypothesis() {
      auto hypo = std::unique_ptr<CsvParseSettingsDetector>(
                      new CsvParseSettingsDetector(*this));
      hypo->nextHypothesis_ = std::move(this->nextHypothesis_);
      this->nextHypothesis_ = std::move(hypo);
      return nextHypothesis_.get();
    }

    void replaceBuffer(Buffer newBuffer) {
      auto delta = static_cast<const char*>(newBuffer.rptr()) -
                   static_cast<const char*>(buffer_.rptr());
      buffer_ = std::move(newBuffer);
      ch_ += delta;
      sol_ += delta;
      chBeforeWhitespace_ += delta;
      eof_ = static_cast<const char*>(buffer_.rptr()) + buffer_.size();
      if (nextHypothesis_) {
        nextHypothesis_->replaceBuffer(buffer_);
      }
    }


    CsvParseSettingsDetector* setFallback() {
      isFallbackTheory_ = true;
      return this;
    }

    void expandBuffer() {
      // ...
    }


    CsvParseSettingsDetector* bestHypothesis(
        CsvParseSettingsDetector* a, CsvParseSettingsDetector* b)
    {
      xassert(a && b);
      // TODO: Perform some smart comparisons here...
      return a;
    }


    void parseAll() {
      while (nLinesRead_ < maxLinesToRead_) {
        xassert(counts_);
        // Clear the array where character counts are accumulated
        std::memset(counts_, 0, 128 * sizeof(int));
        parseLine();
        nLinesRead_++;
        counts_ += 128;
        if (error_) return;
        if (done_) break;
        if (ch_ == eof_ && !moreDataAvailable_) break;
        // TODO: maybe perform early stopping if all params were already detected
      }
      if (separatorKind_ == SeparatorKind::AUTO) {
        counts_ -= nLinesRead_ * 128;
        finalChooseSeparator();
      }
      if (newlineKind_ == NewlineKind::AUTO) {
        newlineKind_ = NewlineKind::NOCR;
      }
      if (quoteKind_ == QuoteKind::AUTO) {
        quoteKind_ = QuoteKind::DOUBLE;
      }
      if (quoteRule_ == QuoteRule::AUTO) {
        quoteRule_ = QuoteRule::DOUBLED;
      }
    }


    void parseLine() {
      // First, we skip whitespace at the start of the line, and handle the
      // blank lines.
      while (true) {
        sol_ = ch_;
        atStartOfLine_ = true;
        atEndOfLine_ = false;
        skipWhitespace();  // may set atEndOfLine_
        if (atEndOfLine_) {
          if (skipBlankLines_ || nLinesRead_ == 0) continue;
          done_ = true;
          return;
        }
        break;
      }
      xassert(atStartOfLine_ && !atEndOfLine_);
      chBeforeWhitespace_ = nullptr;
      do {
        while (ch_ < eof_) {
          xassert(!atEndOfLine_);
          xassert(!atWhitespace());
          xassert(!error_);
          bool ok = parseAndCheckQuotedField() ||
                    parseSeparator() ||
                    skip1Char();
          xassert(ok);
          if (!error_) skipWhitespace();
          if (atEndOfLine_ || error_) return;
        }
      } while (moreDataAvailable());
    }


    /**
      * Attempt to detect a quoted field at the current parsing location. If
      * there is such a field, this method will move over it, also consume
      * any whitespace that follows, and finally return true. If there is no
      * detectable quoted field, this method won't change the parse location
      * and will return false. If it is ambiguous whether we are at a quoted
      * field or not, this method will spawn new hypotheses for all possible
      * alternatives. In addition, this method may return with an error status,
      * in which case the parse location may or may not be modified.
      *
      * Prerequisites: the following fields must be correctly set:
      *   - `atStartOfLine_`
      *   - `atSeparator_`
      *   - `chBeforeWhitespace_` (may be NULL if atStartOfLine_ is true)
      *
      * Returns true if a quoted field was found, moving the parse location
      * to the first character after the closing quote. Returns false if
      * there is no quoted field at the current position, or if there was an
      * error during parsing.
      *
      * This method may create additional hypotheses and/or resolve several
      * AUTO parameters, including `quoteKind_`, `quoteRule_`, `separatorKind_`
      * and `separatorChar_`.
      */
    bool parseAndCheckQuotedField() {
      return atQuoteCharacter() &&
             validateBeforeQuoted() &&
             manageHypothesesBeforeQuoted() &&
             parseQuotedField() &&
             validateAfterQuoted() &&
             !error_;
    }


    /**
      * Check whether we are at a quote character compatible with the
      * current `quoteKind_`.
      */
    bool atQuoteCharacter() {
      xassert(ch_ < eof_);
      const char quote = *ch_;
      return ((quote == '\"') && (quoteKind_ == QuoteKind::DOUBLE ||
                                  quoteKind_ == QuoteKind::AUTO ||
                                  quoteKind_ == QuoteKind::NOSINGLE)) ||
             ((quote == '\'') && (quoteKind_ == QuoteKind::SINGLE ||
                                  quoteKind_ == QuoteKind::AUTO ||
                                  quoteKind_ == QuoteKind::NODOUBLE)) ||
             ((quote == '`')  && (quoteKind_ == QuoteKind::ITALIC));
    }


    /**
      * Check that the quote character `*ch_` can begin a quoted field,
      * based on the content that came before: a quoted field may only
      * appear after a separator (or at the start of line).
      *
      * Returns true if quoted field is valid here, otherwise returns
      * false and sets `quoteKind_` to indicate that `*ch_` cannot
      * be a valid quote symbol (because a "naked" quote cannot appear
      * in any unquoted field).
      */
    bool validateBeforeQuoted() {
      if (atStartOfLine_ || atSeparator_) return true;

      const char quote = *ch_;
      if (quote == '\"') {
        if (quoteKind_ == QuoteKind::DOUBLE) error_ = true;
        if (quoteKind_ == QuoteKind::AUTO) quoteKind_ = QuoteKind::NODOUBLE;
        if (quoteKind_ == QuoteKind::NOSINGLE) quoteKind_ = QuoteKind::NONE;
      }
      if (quote == '\'') {
        if (quoteKind_ == QuoteKind::SINGLE) error_ = true;
        if (quoteKind_ == QuoteKind::AUTO) quoteKind_ = QuoteKind::NOSINGLE;
        if (quoteKind_ == QuoteKind::NODOUBLE) quoteKind_ = QuoteKind::NONE;
      }
      if (quote == '`') error_ = true;
      return false;
    }


    /**
      * This method is called when we have encountered a valid quote character
      * after a separator (or at start of line), and therefore this is likely
      * a quoted field. However, the alternative hypotheses must also be
      * considered.
      *
      * This method therefore disambiguates the `quoteKind_` and
      * `separatorKind_` parameters, creating alternative hypotheses if
      * necessary.
      *
      * Returns true if no error has occurred.
      */
    bool manageHypothesesBeforeQuoted() {
      const char quote = *ch_;
      if (quote == '\"' && quoteKind_ != QuoteKind::DOUBLE) {
        newHypothesis()
            ->setFallback()
            ->setQuoteKind(quoteKind_ == QuoteKind::AUTO? QuoteKind::NODOUBLE
                                                        : QuoteKind::NONE);
        quoteKind_ = QuoteKind::DOUBLE;
      }
      if (quote == '\'' && quoteKind_ != QuoteKind::SINGLE) {
        newHypothesis()
            ->setFallback()
            ->setQuoteKind(quoteKind_ == QuoteKind::AUTO? QuoteKind::NOSINGLE
                                                        : QuoteKind::NONE);
        quoteKind_ = QuoteKind::SINGLE;
      }

      if (separatorKind_ == SeparatorKind::AUTO && !atStartOfLine_) {
        xassert(chBeforeWhitespace_ != nullptr);
        bool hasWhitespace = (ch_ > chBeforeWhitespace_ + 1);
        if (hasWhitespace) {
          newHypothesis()->setSeparatorKind(SeparatorKind::WHITESPACE);
          int mask = 0;
          for (const char* ch = chBeforeWhitespace_ + 1; ch < ch_; ch++) {
            char c = *ch;
            if (c == ' ' || c == '\t' || c == '\f' || c == '\v') {
              if (mask & (1 << (c - 9))) continue;
              mask |= (1 << (c - 9));
              newHypothesis()->setSeparatorChar(c);
            }
          }
        }
        char sep = *chBeforeWhitespace_;
        if (separatorLikelihood[static_cast<int>(sep)] == 0) {
          // Oops, the "separator" character before the whitespace turned out
          // to be not a valid separator, invalidating the current hypothesis.
          // While technically we should have checked for this at
          // `validateBeforeQuoted`, it's much easier to do it here.
          error_ = true;
          return false;
        }
        separatorKind_ = SeparatorKind::CHAR;
        separatorChar_ = sep;
      }

      // Post-condition: the quote character matches the `quoteKind_` param.
      xassert(quote == '\"'? quoteKind_ == QuoteKind::DOUBLE :
              quote == '\''? quoteKind_ == QuoteKind::SINGLE :
              quote == '`'?  quoteKind_ == QuoteKind::ITALIC : false);
      return true;
    }


    /**
      * Called by `parseAndCheckQuotedField()`, the job of this method
      * is to actually read the quoted field, advancing the current parse
      * location.
      *
      * Returns true if parsing was successful, and false otherwise.
      */
    bool parseQuotedField() {
      char quote = *ch_++;
      xassert(quote == '"' || quote == '\'' || quote == '`');
      atSeparator_ = false;
      do {
        while (ch_ < eof_) {
          char c = *ch_++;
          if (c == quote) {
            if (quoteRule_ == QuoteRule::ESCAPED) {
              return true;
            }
            if (ch_ == eof_ && moreDataAvailable()) {
              ch_--;
              continue;
            }
            bool nextCharIsQuote = (ch_ < eof_) && (*ch_ == quote);
            if (nextCharIsQuote) {
              if (quoteRule_ == QuoteRule::AUTO) {
                quoteRule_ = QuoteRule::DOUBLED;
              }
              ch_++;  // skip the second quote
            } else {
              return true;  // normal return
            }
          }
          if (c == '\\') {
            switch (quoteRule_) {
              case QuoteRule::ESCAPED: ch_++; break;
              case QuoteRule::DOUBLED: break;
              case QuoteRule::AUTO: {
                // Only r'\"' sequence triggers detection of the quote rule
                bool nextCharIsQuote = (ch_ < eof_) && (*ch_ == quote);
                if (nextCharIsQuote) {
                  quoteRule_ = QuoteRule::ESCAPED;
                }
                ch_++;  // skip the next character regardless
              }
            }
          }
        }
      } while (moreDataAvailable());
      // No more data, but the quoted field hasn't finished: this is invalid.
      return error();
    }


    /**
      * Called after a quoted field was parsed, this method verifies that
      * what follows is valid under the current hypothesis. Namely, after
      * a quoted field there could be some whitespace, followed by a
      * separator or an end of line.
      *
      * Returns true (without advancing the parse position) if everything is
      * ok, and sets an error status + returns false otherwise.
      */
    bool validateAfterQuoted() {
      const char* ch0 = ch_;
      bool hasWhitespace = skipWhitespace();
      xassert(atEndOfLine_ || ch_ < eof_);
      switch (separatorKind_) {
        case SeparatorKind::AUTO: {
          if (atEndOfLine_) {
            // This could have happened only if the quoted field started at the
            // beginning of a line.
            separatorKind_ = SeparatorKind::NONE;
          } else {
            if (hasWhitespace) {
              newHypothesis()->setSeparatorKind(SeparatorKind::WHITESPACE);
              int mask = 0;
              for (const char* ch = ch0; ch < ch_; ch++) {
                char c = *ch;
                if (c == ' ' || c == '\t' || c == '\f' || c == '\v') {
                  if (mask & (1 << (c - 9))) continue;
                  mask |= (1 << (c - 9));
                  newHypothesis()->setSeparatorChar(c);
                }
              }
            }
            char sep = *ch_;
            if (separatorLikelihood[static_cast<int>(sep)] == 0) return error();
            separatorKind_ = SeparatorKind::CHAR;
            separatorChar_ = sep;
          }
          break;
        }
        case SeparatorKind::WHITESPACE: {
          if (!hasWhitespace) return error();
          break;
        }
        case SeparatorKind::NONE: {
          if (!atEndOfLine_) return error();
          break;
        }
        case SeparatorKind::CHAR: {
          if (!atEndOfLine_) {
            if (*ch_ != separatorChar_) return error();
          }
          break;
        }
        case SeparatorKind::STRING: {
          // TODO
          break;
        }
      }
      // Revert the effects of `skipWhitespace()`.
      ch_ = ch0;
      atEndOfLine_ = false;
      return true;
    }


    /**
      * Attempt to detect a separator at the current parsing location. If
      * there is one, this method will move over it, stopping at the next
      * character after. It will also return `true` and set `atSeparator_`
      * flag to true. If there is no separator at the current location, the
      * method returns false and sets `atSeparator_` to false. In addition,
      * the method returns false if there was any error.
      *
      * The case of AUTO separator has special handling. Basically, this case
      * means that it is unknown whether `separatorKind_` is `WHITESPACE`,
      * `NONE` or `CHAR`, and in the latter case the value of `separatorChar_`
      * is likewise unknown. This method does not attempt to disambiguate
      * between these possibilities
      */
    bool parseSeparator() {
      switch (separatorKind_) {
        case SeparatorKind::NONE: {
          // Nothing is a separator in this case...
          break;
        }
        case SeparatorKind::CHAR: {
          do {
            if (ch_ < eof_) {
              atSeparator_ = (*ch_ == separatorChar_);
              ch_ += atSeparator_;
              return atSeparator_;
            }
          } while (moreDataAvailable());
          break;
        }
        case SeparatorKind::STRING: {
          auto n = separatorString_.size();
          do {
            if (ch_ + n <= eof_) {
              const char* sepStr = separatorString_.data();
              atSeparator_ = (std::memcmp(ch_, sepStr, n) == 0);
              if (atSeparator_) ch_ += n;
              return atSeparator_;
            }
          } while (moreDataAvailable());
          break;
        }
        case SeparatorKind::WHITESPACE: {
          atSeparator_ = skipWhitespace();
          return atSeparator_;
        }
        case SeparatorKind::AUTO: {
          do {
            if (ch_ < eof_) {
              counts_[static_cast<int>(*ch_)]++;
              ch_++;
              atSeparator_ = true;
              return true;
            }
          } while (moreDataAvailable());
          break;
        }
      }
      atSeparator_ = false;
      return false;
    }


    /**
      * Used during line parsing, this method is called when a CR character
      * is encountered. Sets `atEndOfLine_` flag to true if a line break should occur
      * at current parsing position.
      *
      * This method also handles the CRLF sequence.
      *
      * In the absense of CRLF sequence, a standalone \r character could
      * either mean a line break, or it could be a regular character. Thus,
      * in the autodetect mode we set `newlineKind_` to `QANY`, and also add
      * a new hypothesis with `newlineKind_` set to `NOCR`.
      */
    void parseCR() {
      xassert(*ch_ == '\r');
      if (newlineKind_ == NewlineKind::LF) return;
      if (newlineKind_ == NewlineKind::CR) goto end;

      if ((ch_ < eof_) && (*ch_ == '\n')) {
        if (newlineKind_ == NewlineKind::QCR) { error_ = true; return; }
        if (newlineKind_ == NewlineKind::AUTO) {
          newlineKind_ = NewlineKind::NOCR;
        }
        ch_++; // skip the LF character
      }
      else {
        if (newlineKind_ == NewlineKind::NOCR) return;
        if (newlineKind_ == NewlineKind::CRLF) return;
        if (newlineKind_ == NewlineKind::AUTO) {
          newlineKind_ = NewlineKind::QCR;
          // create new hypothesis that QCR is incorrect
          auto hypo = newHypothesis();
          hypo->newlineKind_ = NewlineKind::NOCR;
          hypo->isFallbackTheory_ = true;
        }
      }
      end:
      atEndOfLine_ = true;
      ch_++;
    }


    /**
      * Skip any whitespace characters at the current parsing location, and
      * return true if there were any.
      *
      * If this method encounters a newline, it will skip over the newline
      * and return immediately, setting flag `atEndOfLine_` to true.
      */
    bool skipWhitespace() {
      xassert(!atEndOfLine_);
      chBeforeWhitespace_ = ch_ - 1;
      bool ret = false;
      do {
        while (ch_ < eof_) {
          char c = *ch_;
          if (c == '\n') parseLF();
          if (c == '\r') parseCR();
          // Methods parseLF/parseCR either set `atEndOfLine_` to true and advance
          // pointer `ch_`, or do nothing, in which case character `c` should
          // be considered a normal whitespace character.
          if (atEndOfLine_) goto end;
          xassert(c == *ch_);

          if (skipWhitespace1Character()) {
            if (c >= 0 && !atStartOfLine_) {
              counts_[static_cast<int>(c)]++;
            }
            ret = true;
          } else {
            goto end;
          }
        }
      } while (moreDataAvailable());
      end:
      if (ret) {
        counts_[WHITESPACE]++;
      }
      xassert(atEndOfLine_ || !atWhitespace());
      return ret;
    }


    bool skip1Char() {
      ch_++;
      return true;
    }


    /**
      * If the character at the current parse location is whitespace (\s) then
      * advance the parse position over it and return true, otherwise return
      * false. This method also handles unicode whitespace characters, assuming
      * the input is in UTF8.
      */
    bool skipWhitespace1Character() {
      char c = *ch_;
      if ((c == ' ') || (c <= '\r' && c >= '\t')) {
        ch_++;
        return true;
      }
      if (c < 0) {
        auto c0 = static_cast<unsigned>(c);
        if (c0 == 0xa0 && hasBytes(2) && ch_[1] == '\xc0') {
          ch_ += 2;
          return true;
        }
        if (c0 >= 0xe1 && hasBytes(3)) {
          auto c1 = static_cast<unsigned>(ch_[1]);
          auto c2 = static_cast<unsigned>(ch_[2]);
          auto code = (c0 << 16) + (c1 << 8) + (c2);
          if (code == 0xe19a80 ||
              (code >= 0xe28080 && code <= 0xe2808a       ) ||
              code == 0xe280a8 ||
              code == 0xe280a9 ||
              code == 0xe280af ||
              code == 0xe28a9f ||
              code == 0xe38080 ||
              code == 0xefbbbf) {
            ch_ += 3;
            return true;
          }
        }
      }
      return false;
    }


    bool atWhitespace() {
      auto ch0 = ch_;
      auto ret = skipWhitespace1Character();
      ch_ = ch0;
      return ret;
    }


    /**
      * Used during whitespace parsing, this method is called when an LF
      * character is encountered. It sets `atEndOfLine_ = true` and advances the
      * parse position if a line break occurs here.
      *
      * \n is the most common newline character. If we're autodetecting
      * newlines, then the mode will be set to NewlineKind::NOCR (i.e. \r's
      * will NOT be considered newlines).
      *
      * On the other hand, if we encountered \r's before, and thus have had
      * set `newlineKind_` to NewlineKind::QANY, then that setting can now
      * be deemed inadmissible.
      */
    void parseLF() {
      xassert(*ch_ == '\n');
      if (newlineKind_ == NewlineKind::CR) return;
      if (newlineKind_ == NewlineKind::CRLF) return;
      if (newlineKind_ == NewlineKind::AUTO) {
        newlineKind_ = NewlineKind::NOCR;
      }
      if (newlineKind_ == NewlineKind::QCR) {
        error_ = true;
      }
      ch_++;
      atEndOfLine_ = true;
    }


    bool error() {
      error_ = true;
      return false;
    }


    bool moreDataAvailable() {
      if (moreDataAvailable_) {
        expandBuffer();
        return true;
      }
      return false;
    }


    bool hasBytes(int n) {
      do {
        if (ch_ + n < eof_) return true;
      } while (moreDataAvailable());
      return false;
    }


    void finalChooseSeparator() {
      // std::cout << "### finalChooseSeparator\n";
      // std::cout << "### counts[0] = "; for (int i = 0; i < 128; i++) std::cout << counts_[i]; std::cout << "\n";
      // std::cout << "### counts[1] = "; for (int i = 0; i < 128; i++) std::cout << counts_[i+128]; std::cout << "\n";
      xassert(separatorKind_ == SeparatorKind::AUTO);
      char bestSeparator = '\xff';
      double bestScore = 0;
      for (int i = 0; i < 128; i++) {
        int likelihood = separatorLikelihood[i];
        if (i == WHITESPACE) likelihood = separatorLikelihood[0x20];
        if (likelihood == 0) continue;
        int count0 = counts_[i];
        int count1 = counts_[i + 128];
        if (count1 == 0) continue;
        bool same = true;
        for (int j = 2; j < nLinesRead_; j++) {
          int countj = counts_[i + 128*j];
          if (countj != count1) {
            same = false;
            if (unevenRows_) {
              if (countj > count1) count1 = countj;
            } else {
              break;
            }
          }
        }
        int mul = same? (count0 == count1? 3 : 2)
                      : (unevenRows_? 1 : 0);
        if (!mul) continue;
        double score = likelihood * (mul + 0.5 * std::log10(count1 + 1));
        if (score > bestScore) {
          bestSeparator = static_cast<char>(i);
          bestScore = score;
          // std::cout << "### sep = '" << bestSeparator << "', score = " << score << "\n";
        }
      }

      if (bestScore > 0) {
        separatorKind_ = SeparatorKind::CHAR;
        separatorChar_ = bestSeparator;
        if (bestSeparator == WHITESPACE) {
          separatorKind_ = SeparatorKind::WHITESPACE;
        }
      } else {
        separatorKind_ = SeparatorKind::NONE;
      }
    }

    friend void detectCsvParseSettings(CsvParseSettings&, Buffer);
};



void detectCsvParseSettings(CsvParseSettings& params, Buffer buffer) {
  CsvParseSettingsDetector in;
  in.replaceBuffer(buffer);
  in.newlineKind_ = params.newlineKind;
  in.quoteKind_ = params.quoteKind;
  in.quoteRule_ = params.quoteRule;
  in.separatorKind_ = params.separatorKind;
  in.separatorChar_ = params.separatorChar;
  in.separatorString_ = params.separatorString;
  in.skipBlankLines_ = params.skipBlankLines;
  in.unevenRows_ = params.unevenRows;

  auto out = in.detect();
  params.newlineKind = out->newlineKind_;
  params.quoteKind = out->quoteKind_;
  params.quoteRule = out->quoteRule_;
  params.separatorKind = out->separatorKind_;
  params.separatorChar = out->separatorChar_;
}




}}  // namespace dt::read2
