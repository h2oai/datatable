//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_CSV_READER_h
#define dt_CSV_READER_h
#include <Python.h>
#include <limits>         // std::numeric_limits
#include <memory>         // std::unique_ptr
#include <string>         // std::string
#include <vector>         // std::vector
#include "column.h"       // Column
#include "datatable.h"    // DataTable
#include "memrange.h"     // MemoryRange
#include "writebuf.h"     // WritableBuffer
#include "utils/array.h"
#include "utils/pyobj.h"
#include "utils/shared_mutex.h"

enum PT : uint8_t;
enum RT : uint8_t;
class GenericReader;


//------------------------------------------------------------------------------
// GReaderColumn
//------------------------------------------------------------------------------

/**
 * Information about a single input column in a GenericReader. An "input column"
 * means a collection of fields at the same index on every line in the input.
 * All these fields are assumed to have a common underlying type.
 *
 * An input column usually translates into an output column in a DataTable
 * returned to the user. The exception to this are "dropped" columns. They are
 * marked with `presentInOutput = false` flag (and have rtype RT::RDrop).
 *
 * Implemented in "csv/reader_utils.cc".
 */
class GReaderColumn {
  private:
    std::string name;
    MemoryRange databuf;
    MemoryWritableBuffer* strbuf;
    RT rtype;

  public:
    PT ptype;
    bool typeBumped;
    bool presentInOutput;
    bool presentInBuffer;
    int32_t : 24;

  public:
    GReaderColumn();
    GReaderColumn(const GReaderColumn&) = delete;
    GReaderColumn(GReaderColumn&&);
    virtual ~GReaderColumn();

    // Column's data
    void allocate(size_t nrows);
    const void* data_r() const;
    void* data_w();
    WritableBuffer* strdata_w();
    MemoryRange extract_databuf();
    MemoryRange extract_strbuf();

    // Column's name
    const std::string& get_name() const noexcept;
    void set_name(std::string&& newname) noexcept;
    void swap_names(GReaderColumn& other) noexcept;
    const char* repr_name(const GenericReader& g) const;  // static ptr

    // Column's type(s)
    bool is_string() const;
    bool is_dropped() const;
    PT get_ptype() const;
    SType get_stype() const;
    void set_rtype(int64_t it);
    const char* typeName() const;
    size_t elemsize() const;
    void convert_to_str64();
    PyObj py_descriptor() const;

    size_t memory_footprint() const;
};




//------------------------------------------------------------------------------
// GReaderColumns
//------------------------------------------------------------------------------

class GReaderColumns {
  private:
    std::vector<GReaderColumn> cols;
    size_t allocnrows;

  public:
    GReaderColumns() noexcept;

    size_t size() const noexcept;
    size_t get_nrows() const noexcept;
    void set_nrows(size_t nrows);

    GReaderColumn& operator[](size_t i) &;
    const GReaderColumn& operator[](size_t i) const &;

    void add_columns(size_t n);

    std::unique_ptr<PT[]> getTypes() const;
    void saveTypes(std::unique_ptr<PT[]>& types) const;
    bool sameTypes(std::unique_ptr<PT[]>& types) const;
    void setTypes(const std::unique_ptr<PT[]>& types);
    void setType(PT type);
    const char* printTypes() const;

    size_t nColumnsInOutput() const;
    size_t nColumnsInBuffer() const;
    size_t nColumnsToReread() const;
    size_t nStringColumns() const;
    size_t totalAllocSize() const;
};




//------------------------------------------------------------------------------
// GenericReader (main class)
//------------------------------------------------------------------------------

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
    bool    report_progress;
    bool    fill;
    bool    blank_is_na;
    bool    number_is_na;
    bool    override_column_types;
    bool    printout_anonymize;
    bool    printout_escape_unicode;
    size_t : 48;
    const char* skip_to_string;
    const char* const* na_strings;

  //---- Runtime parameters ----
  // line:
  //   Line number (within the original input) of the `offset` pointer.
  //
  public:
    MemoryRange input_mbuf;
    const char* sof;
    const char* eof;
    size_t line;
    int32_t fileno;
    bool cr_is_newline;
    int : 24;
    GReaderColumns columns;

  private:
    PyObj logger;
    PyObj freader;
    PyObj src_arg;
    PyObj file_arg;
    PyObj text_arg;
    PyObj skipstring_arg;
    PyObj tempstr;

    // If `trace()` cannot display a message immediately (because it was not
    // sent from the main thread), it will be temporarily stored in this
    // variable. Call `emit_delayed_messages()` from the master thread to send
    // these messages to the frontend.
    mutable std::string delayed_message;
    // std::string delayed_warning;

  //---- Public API ----
  public:
    GenericReader(const PyObj& pyreader);
    GenericReader& operator=(const GenericReader&) = delete;
    virtual ~GenericReader();

    DataTablePtr read();

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

    const PyObj& pyreader() const { return freader; }
    bool get_verbose() const { return verbose; }
    void trace(const char* format, ...) const;
    void warn(const char* format, ...) const;
    void progress(double progress, int status = 0);
    void emit_delayed_messages();

    const char* repr_source(const char* ch, size_t limit) const;
    const char* repr_binary(const char* ch, const char* end, size_t limit) const;

  // Helper functions
  private:
    void init_verbose();
    void init_nthreads();
    void init_fill();
    void init_maxnrows();
    void init_skiptoline();
    void init_sep();
    void init_dec();
    void init_quote();
    void init_showprogress();
    void init_header();
    void init_nastrings();
    void init_skipstring();
    void init_stripwhite();
    void init_skipblanklines();
    void init_overridecolumntypes();

  protected:
    void open_input();
    void detect_and_skip_bom();
    void skip_initial_whitespace();
    void skip_trailing_whitespace();
    void skip_to_line_number();
    void skip_to_line_with_string();
    void decode_utf16();
    void report_columns_to_python();

    void _message(const char* method, const char* format, va_list args) const;

    DataTablePtr read_empty_input();
    void detect_improper_files();

  //---- Inherited API ----
  protected:
    GenericReader(const GenericReader&);
    DataTablePtr makeDatatable();
};



//------------------------------------------------------------------------------
// Helper classes
//------------------------------------------------------------------------------

/**
 * "Relative string": a string defined as an offset+length relative to some
 * anchor point (which has to be provided separately). This is the internal data
 * structure for reading strings from a file.
 */
struct RelStr {
  uint32_t offset;
  int32_t length;

  bool isna() { return length == std::numeric_limits<int32_t>::min(); }
  void setna() { length = std::numeric_limits<int32_t>::min(); }
};



union field64 {
  int8_t   int8;
  int32_t  int32;
  int64_t  int64;
  uint8_t  uint8;
  uint32_t uint32;
  uint64_t uint64;
  float    float32;
  double   float64;
  RelStr   str32;
};



//------------------------------------------------------------------------------
// ChunkCoordinates struct
//------------------------------------------------------------------------------

/**
 * Helper struct containing the beginning / end for a chunk.
 *
 * Additional flags `true_start` and `true_end` indicate whether the beginning /
 * end of the chunk are known with certainty or guessed.
 */
struct ChunkCoordinates {
  const char* start;
  const char* end;
  bool true_start;
  bool true_end;
  size_t : 48;

  ChunkCoordinates()
    : start(nullptr), end(nullptr), true_start(false), true_end(false) {}
  ChunkCoordinates(const char* s, const char* e)
    : start(s), end(e), true_start(false), true_end(false) {}
  ChunkCoordinates& operator=(const ChunkCoordinates& cc) {
    start = cc.start;
    end = cc.end;
    true_start = cc.true_start;
    true_end = cc.true_end;
    return *this;
  }

  operator bool() const { return end == nullptr; }
};



//------------------------------------------------------------------------------
// LocalParseContext
//------------------------------------------------------------------------------

/**
 * This is a helper class for ChunkedDataReader (see below). It carries
 * variables that will be local to each thread during the parallel reading of
 * the input.
 *
 * tbuf
 *   Output buffer. Within the buffer the data is stored in row-major order,
 *   i.e. in the same order as in the original CSV file. We view the buffer as
 *   a rectangular grid having `tbuf_ncols * tbuf_nrows` elements (+1 extra).
 *
 * tbuf_ncols, tbuf_nrows
 *   Dimensions of the output buffer.
 *
 * used_nrows
 *   Number of rows of data currently stored in `tbuf`. This can never exceed
 *   `tbuf_nrows`.
 *
 * row0
 *   Starting row index within the output DataTable for the current data chunk.
 */
class LocalParseContext {
  public:
    struct SInfo { size_t start, size, write_at; };

    dt::array<field64> tbuf;
    dt::array<uint8_t> sbuf;
    dt::array<SInfo> strinfo;
    size_t tbuf_ncols;
    size_t tbuf_nrows;
    size_t used_nrows;
    size_t row0;

  public:
    LocalParseContext(size_t ncols, size_t nrows);
    virtual ~LocalParseContext();

    virtual void push_buffers() = 0;
    virtual void read_chunk(const ChunkCoordinates&, ChunkCoordinates&) = 0;
    virtual void orderBuffer() = 0;

    size_t get_nrows() const;
    void set_nrows(size_t n);
    void allocate_tbuf(size_t ncols, size_t nrows);
};

typedef std::unique_ptr<LocalParseContext> LocalParseContextPtr;




//------------------------------------------------------------------------------
// ChunkedDataReader
//------------------------------------------------------------------------------

/**
 * This class' responsibility is to execute parallel reading of its input,
 * ensuring that the data integrity is maintained.
 */
class ChunkedDataReader {
  protected:
    size_t chunkSize;
    size_t chunkCount;
    const char* inputStart;
    const char* inputEnd;
    const char* lastChunkEnd;
    double lineLength;

  protected:
    GenericReader& g;
    dt::shared_mutex shmutex;
    size_t nrows_max;
    size_t nrows_allocated;
    size_t nrows_written;
    int nthreads;
    int : 32;

  public:
    ChunkedDataReader(GenericReader& reader, double len);
    ChunkedDataReader(const ChunkedDataReader&) = delete;
    ChunkedDataReader& operator=(const ChunkedDataReader&) = delete;
    virtual ~ChunkedDataReader() {}

    /**
     * Main function that reads all data from the input.
     */
    virtual void read_all();


  protected:
    /**
     * Determine coordinates (start and end) of the `i`-th chunk. The index `i`
     * must be in the range [0..chunkCount).
     *
     * The optional `LocalParseContext` instance may be needed for some
     * implementations of `ChunkedDataReader` in order to perform additional
     * parsing using a thread-local context.
     *
     * The method `compute_chunk_boundaries()` may be called in-parallel,
     * assuming that different invocation receive different `ctx` objects.
     */
    ChunkCoordinates compute_chunk_boundaries(
      size_t i, LocalParseContext* ctx) const;

    /**
     * This method can be overridden in derived classes in order to implement
     * more advanced chunk boundaries detection. This method will be called from
     * within `compute_chunk_boundaries()` only.
     * This method should modify `cc` by-reference; making sure not to alter
     * `start` / `end` if the flags `true_start` / `true_end` are set.
     */
    virtual void adjust_chunk_coordinates(
      ChunkCoordinates& cc, LocalParseContext* ctx) const;

    /**
     * Return an instance of a `LocalParseContext` class. Implementations of
     * `ChunkedDataReader` are expected to override this method to return
     * appropriate subclasses of `LocalParseContext`.
     */
    virtual LocalParseContextPtr init_thread_context() = 0;


  private:
    void determine_chunking_strategy();

    /**
     * Return the fraction of the input that was parsed, as a number between
     * 0 and 1.0.
     */
    double work_done_amount() const;

    /**
     * Reallocate output columns (i.e. `g.columns`) to the new number of rows.
     * Argument `i` contains the index of the chunk that was read last (this
     * helps with determining the new number of rows), and `new_allocnrow` is
     * the minimal number of rows to reallocate to.
     *
     * This method is thread-safe.
     */
    void realloc_output_columns(size_t i, size_t new_allocnrow);

    /**
     * Ensure that the chunks were placed properly. This method must be called
     * from the #ordered section. It takes three arguments: `acc` the *actual*
     * coordinates of the chunk just read; `xcc` the coordinates that were
     * *expected*; and `ctx` the thread-local parse context.
     *
     * If the chunk was ordered properly (i.e. started reading from the place
     * were the previous chunk ended), then this method updates the internal
     * `lastChunkEnd` variable and returns.
     *
     * Otherwise, it re-parses the chunk with correct coordinates. When doing
     * so, it will set `xcc.true_start` to true, thus informing the chunk
     * parser that the coordinates that it received are true.
     */
    void order_chunk(ChunkCoordinates& acc, ChunkCoordinates& xcc,
                     LocalParseContextPtr& ctx);
};



#endif
