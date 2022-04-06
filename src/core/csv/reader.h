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
#ifndef dt_CSV_READER_h
#define dt_CSV_READER_h
#include <memory>           // std::unique_ptr, std::shared_ptr
#include "buffer.h"         // Buffer
#include "progress/work.h"  // dt::progress::work
#include "python/obj.h"     // py::robj, py::oobj
#include "python/list.h"    // py::olist
#include "read/preframe.h"  // dt::read::PreFrame
#include "utils/logger.h"
namespace dt {
namespace read {

extern bool parse_dates;
extern bool parse_times;


// What fread() should do if the input contains multiple sources
enum class FreadMultiSourceStrategy : int8_t {
  Warn,
  Error,
  Ignore,
};

// How to handle the situation when some of the iread's inputs cannot
// be parsed (perhaps because they are not in CSV or other
// recognizable formats).
//
// Warn - skip the invalid inputs, but produce a warning to the user
// Error - throw an error
// Ignore - silently skip all "bad" inputs
// Store - bad inputs are returned as exception objects
//
enum class IreadErrorHandlingStrategy : int8_t {
  Warn,
  Error,
  Ignore,
  Store
};


/**
 * GenericReader: base class for reading text files in multiple formats. This
 * class attempts to parse different formats in turn, until it succeeds.
 */
class GenericReader
{
  //---- Input parameters ----
  // nthreads:
  //   Number of threads to use; 0 means use maximum possible, negative number
  //   means use that many less than the maximum. The default is 0.
  // verbose:
  //   Print diagnostic information about the parsing process.
  // sep:
  //   The separator character. The default is \xFF, which means "auto-detect".
  //   The `sep` may also be '\n', which indicates "single-line mode" (i.e. the
  //   lines should not be split into fields). The value `sep=' '` is likewise
  //   special: multiple space characters are always treated as a single
  //   separator (i.e. the separator is effectively /\s+/). At the same time the
  //   `sep` cannot be any of [0-9a-zA-Z'"`].
  // dec:
  //   The decimal separator in numbers. Only '.' (default) and ',' are allowed.
  // quote:
  //   The quotation mark. Only '"' (default), '\'' and '`' are allowed. In
  //   addition this can also be '\0', which indicates no quoting is allowed.
  // skip_to_line:
  //   If greater than 1, indicates that the file should be read from starting
  //   from the requested line. The notion of a "line" corresponds to that of a
  //   typical text editor: line counting starts from 1, and a new line starts
  //   whenever one of '\r\n', '\n\r', '\r' or '\n' are encountered. In
  //   particular sequence '\r\r\n' contains two line separators, not one. If
  //   skip_to_line is greater than the available number of lines in the file,
  //   an empty DataTable will be returned.
  // header:
  //   Is the header present? Possible values are 0 (no), 1 (yes), and -128
  //   (auto-detect, default).
  //
  public:
    int32_t nthreads;
    bool    verbose;
    char    sep;
    char    dec;
    char    quote;
    size_t  max_nrows;
    size_t  skip_to_line;
    int8_t  header;
    bool    strip_whitespace;
    bool    skip_blank_lines;
    bool    fill;
    bool    blank_is_na;
    bool    number_is_na;
    FreadMultiSourceStrategy multisource_strategy;
    IreadErrorHandlingStrategy errors_strategy;
    std::string skip_to_string;
    const char** na_strings;
    strvec  na_strings_container;
    std::unique_ptr<const char*[]> na_strings_ptr;
    size_t  memory_limit;
    std::string encoding_;

  //---- Runtime parameters ----
  // line:
  //   Line number (within the original input) of the `offset` pointer.
  //
  public:
    static constexpr size_t WORK_PREPARE = 2;
    static constexpr size_t WORK_READ = 100;
    static constexpr size_t WORK_DECODE_UTF16 = 50;
    std::shared_ptr<dt::progress::work> job;
    Buffer input_mbuf;
    const char* sof;
    const char* eof;
    size_t line;
    int32_t fileno;
    bool cr_is_newline;
    int : 24;
    PreFrame preframe;
    double t_open_input{ 0 };

    log::Logger logger_;
    py::oobj output_;
    const std::string* source_name;

  private:
    py::oobj src_arg;
    py::oobj file_arg;
    py::oobj text_arg;
    py::oobj tempstr;
    py::oobj columns_arg;
    // Temporary directory path to construct C++ `TemporaryFile` objects,
    // when `memory_limit` is set in `dt.fread()`
    std::string tempdir_;
    // Python `TempFiles` object used in `fread.py` for storing temporary
    // files, for instance, when reading archives
    py::oobj tempfiles_;

    // If `trace()` cannot display a message immediately (because it was not
    // sent from the main thread), it will be temporarily stored in this
    // variable. Call `emit_delayed_messages()` from the main thread to send
    // these messages to the frontend.
    mutable std::string delayed_message;
    // std::string delayed_warning;

  //---- Public API ----
  public:
    GenericReader();
    GenericReader(const GenericReader&);
    GenericReader(GenericReader&&) = default;
    GenericReader& operator=(const GenericReader&) = delete;
    virtual ~GenericReader();

    std::string get_tempdir() const;
    py::oobj get_tempfiles() const;
    py::oobj read_buffer(const Buffer&, size_t extra_byte);

    bool has_next() const;
    py::oobj read_next();

    /**
     * Return the pointer to the input data buffer and its size. The method
     * `open_input()` must be called first. The pointer returned may be null
     * if the underlying file is empty -- call `read_empty_input()` to deal
     * with such situation. The memory region
     *
     *    dataptr() .. dataptr() + datasize() - 1
     *
     * is guaranteed to be readable, provided the region is not empty.
     */
    const char* dataptr() const { return sof; }
    size_t datasize() const;

    /**
     * If this method returns true, then it is valid to access byte at the
     * address
     *
     *    dataptr() + datasize()
     *
     * (1 byte past the end of the "main" data region).
     */
    bool extra_byte_accessible() const;

    bool get_verbose() const { return verbose; }
    log::Message d() const;

    const char* repr_source(const char* ch, size_t limit) const;
    const char* repr_binary(const char* ch, const char* end, size_t limit) const;

    // Called once during module initialization
    static void init_options();

  // Helper functions
  public:
    void init_columns    (const py::Arg&);
    void init_dec        (const py::Arg&);
    void init_errors     (const py::Arg&);
    void init_fill       (const py::Arg&);
    void init_header     (const py::Arg&);
    void init_logger     (const py::Arg& arg_logger, const py::Arg& arg_verbose);
    void init_maxnrows   (const py::Arg&);
    void init_multisource(const py::Arg&);
    void init_nastrings  (const py::Arg&);
    void init_nthreads   (const py::Arg&);
    void init_quote      (const py::Arg&);
    void init_sep        (const py::Arg&);
    void init_skipblanks (const py::Arg&);
    void init_skipstring (const py::Arg&);
    void init_skiptoline (const py::Arg&);
    void init_stripwhite (const py::Arg&);
    void init_tempdir    (const py::Arg&);
    void init_memorylimit(const py::Arg&);
    void init_encoding   (const py::Arg&);

  protected:
    void log_file_sample();
    void open_buffer(const Buffer& buf, size_t extra_byte);
    void process_encoding();
    void detect_and_skip_bom();
    void skip_initial_whitespace();
    void skip_trailing_whitespace();
    void skip_to_line_number();
    void skip_to_line_with_string();
    void decode_utf16();
    void report_columns_to_python();

    bool read_csv();
    bool read_empty_input();
    bool read_jay();
    bool detect_improper_files();
};


}}  // namespace dt::read
#endif
