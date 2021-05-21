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


static constexpr bool isWhitespace(char c) {
  return (c == ' ') || (c <= '\r' && c >= '\t');
}




//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

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
    std::string separatorString_;

  public:
    CsvParseSettingsDetector()
      : ch_(nullptr),
        eof_(nullptr),
        sol_(nullptr),
        counts_(nullptr),
        maxLinesToRead_(10),
        nLinesRead_(0),
        error_(false),
        isFallbackTheory_(false) {}

  private:
    CsvParseSettingsDetector(const CsvParseSettingsDetector& other)
      : buffer_(other.buffer_),
        ch_(other.sol_),
        eof_(other.eof_),  // move the parse pointer to start-of-line
        sol_(other.sol_),
        counts_(other.counts_),
        nLinesRead_(other.nLinesRead_),
        moreDataAvailable_(other.moreDataAvailable_),
        newlineKind_(other.newlineKind_),
        quoteKind_(other.quoteKind_),
        quoteRule_(other.quoteRule_) {}

    CsvParseSettingsDetector* newHypothesis() {
      auto hypo = std::unique_ptr<CsvParseSettingsDetector>(
                      new CsvParseSettingsDetector(*this));
      hypo->nextHypothesis_ = std::move(this->nextHypothesis_);
      this->nextHypothesis_ = std::move(hypo);
      return nextHypothesis_.get();
    }

    void replaceBuffer(Buffer newBuffer) {
      auto offset = ch_ - static_cast<const char*>(buffer_.rptr());
      buffer_ = std::move(newBuffer);
      auto ch0 = static_cast<const char*>(buffer_.rptr());
      ch_ = ch0 + offset;
      eof_ = ch0 + buffer_.size();
      if (nextHypothesis_) {
        nextHypothesis_->replaceBuffer(buffer_);
      }
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

    CsvParseSettingsDetector* setFallback() {
      isFallbackTheory_ = true;
      return this;
    }

    void expandBuffer() {
      // ...
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


    CsvParseSettingsDetector* bestHypothesis(
        CsvParseSettingsDetector* a, CsvParseSettingsDetector* b)
    {
      xassert(a && b);
      // TODO: Perform some smart comparisons here...
      return a;
    }


    void parseAll() {
      while (nLinesRead_ < maxLinesToRead_) {
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
    }


    // void parseLine() {
    //   sol_ = ch_;
    //   atEndOfLine_ = false;
    //   while (ch_ < eof_ && !atEndOfLine_) {
    //     char c = *ch_;
    //     // Note: \n and \r characters are considered whitespace
    //     if (isWhitespace(c)) {
    //       auto ch0 = ch_;
    //       bool ws = skipWhitespace();
    //       if (ws) {
    //         for (auto ch = ch0; ch < ch_; ch++) {
    //           if (*ch != '\n')
    //             counts_[static_cast<int>(*ch)]++;
    //         }
    //         counts_[0x0A]++;
    //       }
    //     }
    //     if (false);
    //     // else if (c == '\n') parseLF();
    //     // else if (c == '\r') parseCR();
    //     else if (c == '\"') parseDoubleQuote();
    //     else if (c == '\'') parseSingleQuote();
    //     else if (c == '`' ) parseItalicQuote();
    //     else {
    //       if (c >= 0) {
    //         counts_[static_cast<int>(c)]++;
    //       }
    //       ch_++;  // all other characters: skip over
    //     }
    //     if (error_) return;
    //   }
    // }


    void parseLine() {
      // First, we skip whitespace at the start of the line, and handle the
      // blank lines.
      while (true) {
        sol_ = ch_;
        atStartOfLine_ = true;
        atEndOfLine_ = false;
        skipWhitespace();  // may set atEndOfLine_
        if (atEndOfLine_) {
          if (skipBlankLines_) continue;
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
          bool ok = parseQuotedFieldAndWhitespace()  || error_ ||
                    skipSeparator()                  || error_ ||
                    skipWhitespace()                 || error_ ||
                    atEndOfLine_;
          xassert(ok);
          // char c = *ch_;
          // if (false);
          // else if (c == '\"') parseDoubleQuote();
          // else if (c == '\'') parseSingleQuote();
          // else if (c == '`' ) parseItalicQuote();
          // else {
          //   chBeforeWhitespace_ = ch_;
          //   ch_++;
          //   if (c >= 0) {
          //     if (c != WHITESPACE) {
          //       counts_[static_cast<int>(c)]++;
          //     }
          //   }
          // }
          // skipWhitespace();
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
      */
    bool parseQuotedFieldAndWhitespace() {
      bool ok = atQuoteCharacter() &&
                validateBeforeQuoted() &&
                manageHypothesesBeforeQuoted() &&
                parseQuotedField();
      if (!ok) return false;
      if (error_) return false;

      // After the quoted field was parsed, we verify that what follows is a
      // separator / end of line.
      {
        const char* ch0 = ch_;
        bool hasWhitespace = skipWhitespace();
        xassert(atEndOfLine_ || ch_ < eof_);
        switch (separatorKind_) {
          case SeparatorKind::AUTO: {
            // This could have happened only if the quoted field started at the
            // beginning of a line.
            if (atEndOfLine_) {
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
              if (separatorLikelihood[static_cast<int>(sep)] == 0) goto fail;
              separatorKind_ = SeparatorKind::CHAR;
              separatorChar_ = sep;
            }
            break;
          }
          case SeparatorKind::WHITESPACE: {
            if (!hasWhitespace) goto fail;
            break;
          }
          case SeparatorKind::NONE: {
            if (!atEndOfLine_) goto fail;
            break;
          }
          case SeparatorKind::CHAR: {
            if (!atEndOfLine_) {
              if (*ch_ != separatorChar_) goto fail;
            }
            break;
          }
          case SeparatorKind::STRING: {
            // TODO
            break;
          }
        }
      }

      // THE END
      return true;
      fail: {
        error_ = true;
        return false;
      }
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
      * Called by `parseQuotedFieldAndWhitespace()`, the job of this method
      * is to actually read the quoted field, advancing the current parse
      * location.
      *
      * Returns true if parsing was successful, and false otherwise.
      */
    bool parseQuotedField() {
      char quote = *ch_++;
      xassert(quote == '"' || quote == '\'' || quote == '`');
      start:
      while (ch_ < eof_) {
        char c = *ch_++;
        if (c == quote) {
          bool nextCharIsQuote = (ch_ < eof_) && (*ch_ == quote);
          if (nextCharIsQuote) {
            switch (quoteRule_) {
              case QuoteRule::AUTO:    quoteRule_ = QuoteRule::DOUBLED; FALLTHROUGH;
              case QuoteRule::DOUBLED: ch_++; break;  // skip the next quote char
              case QuoteRule::ESCAPED: return error();
            }
          }
          else if (ch_ == eof_ && quoteRule_ != QuoteRule::ESCAPED && moreDataAvailable_) {
            // Last character in the data chunk is '"', which could potentially be a
            // doubled quote if only we could see the next byte... Request more
            // data to disambiguate.
            ch_--;
            expandBuffer();
          }
          else {
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
      // Reached EOF without finding the closing quote, but there's more data
      // in the buffer. Just to be sure, let's expand the bufffer and try again.
      if (moreDataAvailable_) {
        expandBuffer();
        goto start;
      }
      return error();
    }



    /**
      */
    bool skipSeparator() {
      switch (separatorKind_) {
        case SeparatorKind::AUTO: throw RuntimeError();
        case SeparatorKind::NONE: break;
        case SeparatorKind::CHAR: {
          do {
            if (ch_ < eof_) {
              if (*ch_ == separatorChar_) {
                ch_++;
                return true;
              } else {
                return false;
              }
            }
          } while (moreDataAvailable());
          break;
        }
        case SeparatorKind::STRING: {
          auto n = separatorString_.size();
          do {
            if (ch_ + n <= eof_) {
              if (std::memcmp(ch_, separatorString_.data(), n) == 0) {
                ch_ += n;
                return true;
              } else {
                return false;
              }
            }
          } while (moreDataAvailable());
          break;
        }
        case SeparatorKind::WHITESPACE: {
          return skipWhitespace();
        }
      }
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




    // void checkBeforeQuoted() {
    //   xassert(*ch_ == '"' || *ch_ == '\'' || *ch_ == '`');
    //   const char* ch = ch_ - 1;
    //   bool ws = skipBackWhitespace(&ch, sol_);

    //   bool ok = true;
    //   switch (separatorKind_) {
    //     case SeparatorKind::AUTO: {
    //       if (ws) {
    //         auto hypo = newHypothesis();
    //         hypo->separatorKind_ = SeparatorKind::WHITESPACE;
    //       }
    //       if (ch >= sol_) {
    //         char c = *ch;
    //         if (c >= 0 && separatorLikelihood[static_cast<int>(c)] > 0) {
    //           separatorKind_ = SeparatorKind::CHAR;
    //           separatorChar_ = c;
    //         } else {
    //           ok = false;
    //         }
    //       }
    //       break;
    //     }
    //     case SeparatorKind::NONE: {
    //       ok = (ch < sol_);
    //       break;
    //     }
    //     case SeparatorKind::CHAR: {
    //       ok = (ch >= sol_) && (*ch == separatorChar_);
    //       break;
    //     }
    //     case SeparatorKind::STRING: {
    //       auto n = separatorString_.size();
    //       auto sepStart = ch - n + 1;
    //       ok = (sepStart >= sol_) &&
    //            (std::memcmp(sepStart, separatorString_.data(), n) == 0);
    //       break;
    //     }
    //     case SeparatorKind::WHITESPACE: {
    //       ok = ws;
    //       break;
    //     }
    //   }
    //   if (!ok) error();
    // }


    // void checkAfterQuoted() {
    //   bool ws = skipWhitespace();
    //   xassert(!atEndOfLine_);

    //   bool ok = true;
    //   switch (separatorKind_) {
    //     case SeparatorKind::AUTO: {
    //       if (ws) {
    //         auto hypo = newHypothesis();
    //         hypo->separatorKind_ = SeparatorKind::WHITESPACE;
    //       }
    //       if (ch_ < eof_) {
    //         char c = *ch_;
    //         if (c == '\n') parseLF();
    //         if (c == '\r') parseCR();
    //         if (atEndOfLine_) {
    //           separatorKind_ = SeparatorKind::NONE;
    //         } else if (c >= 0 && separatorLikelihood[static_cast<int>(c)] > 0) {
    //           separatorKind_ = SeparatorKind::CHAR;
    //           separatorChar_ = c;
    //         } else {
    //           ok = false;
    //         }
    //       }
    //       break;
    //     }
    //     case SeparatorKind::NONE: {
    //       if (ch_ < eof_) {
    //         if (*ch_ == '\n') parseLF();
    //         if (*ch_ == '\r') parseCR();
    //         if (!atEndOfLine_) ok = false;
    //       }
    //       break;
    //     }
    //     case SeparatorKind::CHAR: {
    //       ok = (ch_ < eof_) && (*ch_ == separatorChar_);
    //       break;
    //     }
    //     case SeparatorKind::STRING: {
    //       auto n = separatorString_.size();
    //       ok = (ch_ + n <= eof_) &&
    //            (std::memcmp(ch_, separatorString_.data(), n) == 0);
    //       break;
    //     }
    //     case SeparatorKind::WHITESPACE: {
    //       ok = ws;
    //       break;
    //     }
    //   }
    //   if (!ok) error();
    // }


    /**
      * Move the parsing position `*pch` back (up to `sof`), skipping
      * over all whitespace characters. Currently only ASCII whitespace
      * is recognized.
      *
      * This function does nothing if the character at `*pch` is not
      * whitespace. Otherwise, it will move the pointer `*pch` backwards
      * stopping at the first non-whitespace character, or at `sof - 1`.
      *
      * Returns true if any whitespace was skipped, and false otherwise.
      */
    // bool skipBackWhitespace(const char** pch, const char* sof) {
    //   const char* ch = *pch;
    //   while (ch >= sof) {
    //     if (isWhitespace(*ch)) ch--;
    //     else break;
    //   }
    //   if (ch < *pch) {
    //     *pch = ch;
    //     return true;
    //   } else {
    //     return false;
    //   }
    // }


    /**
      * Skip any whitespace characters at the current parsing location, and
      * return true if there were any.
      *
      * If this method encounters a newline, it will skip over the newline
      * and return immediately, setting flag `atEndOfLine_` to true.
      */
    bool skipWhitespace() {
      xassert(!atEndOfLine_);
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

    void doneParsing() {

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
      xassert(separatorKind_ == SeparatorKind::AUTO);
      char bestSeparator = '\xff';
      double bestScore = 0;
      for (int i = 0; i < 128; i++) {
        int likelihood = separatorLikelihood[i];
        if (i == WHITESPACE) likelihood = separatorLikelihood[0x20];
        if (likelihood == 0) continue;
        int count0 = counts_[i];
        int count1 = counts_[i + 128];
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
        if (count1 == 0) continue;
        double score = likelihood * (mul + 0.5 * std::log10(count1 + 1));
        if (score > bestScore) {
          bestSeparator = static_cast<char>(i);
          bestScore = score;
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
};





}}  // namespace dt::read2
