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
#include "memorybuf.h"    // MemoryBuffer
#include "writebuf.h"     // WritableBuffer
#include "utils/pyobj.h"
#include "utils/shared_mutex.h"


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
 * marked with `presentInOutput = false` flag (and have type PT::Drop).
 *
 * Implemented in "csv/reader_utils.cc".
 */
class GReaderColumn {
  private:
    MemoryBuffer* mbuf;

  public:
    std::string name;
    MemoryWritableBuffer* strdata;
    int8_t type;
    bool typeBumped;
    bool presentInOutput;
    bool presentInBuffer;
    int32_t : 32;

  public:
    GReaderColumn();
    GReaderColumn(const GReaderColumn&) = delete;
    GReaderColumn(GReaderColumn&&);
    virtual ~GReaderColumn();
    const char* typeName() const;
    size_t elemsize() const;
    size_t getAllocSize() const;
    bool isstring() const;
    void* data() const { return mbuf->get(); }
    void allocate(size_t nrows);
    MemoryBuffer* extract_databuf();
    MemoryBuffer* extract_strbuf();
    void convert_to_str64();
};




//------------------------------------------------------------------------------
// GReaderColumns
//------------------------------------------------------------------------------

class GReaderColumns : public std::vector<GReaderColumn> {
  private:
    size_t allocnrows;

  public:
    GReaderColumns() noexcept;
    void allocate(size_t nrows);
    std::unique_ptr<int8_t[]> getTypes() const;
    void setType(int8_t type);
    const char* printTypes() const;
    size_t nColumnsInOutput() const;
    size_t nColumnsInBuffer() const;
    size_t nColumnsToReread() const;
    size_t nStringColumns() const;
    size_t totalAllocSize() const;
    size_t nrows() const { return allocnrows; }
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
    int64_t skip_to_line;
    int8_t  header;
    bool    strip_whitespace;
    bool    skip_blank_lines;
    bool    report_progress;
    bool    fill;
    bool    blank_is_na;
    bool    number_is_na;
    int : 8;
    const char* skip_to_string;
    const char* const* na_strings;

  //---- Runtime parameters ----
  // line:
  //   Line number (within the original input) of the `offset` pointer.
  //
  public:
    MemoryBuffer* input_mbuf;
    const char* sof;
    const char* eof;
    int64_t line;
    int32_t fileno;
    int : 32;
    GReaderColumns columns;

  private:
    PyObj logger;
    PyObj freader;
    PyObj src_arg;
    PyObj file_arg;
    PyObj text_arg;
    PyObj skipstring_arg;
    PyObj tempstr;

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

    void open_input();
    void detect_and_skip_bom();
    void skip_initial_whitespace();
    void skip_trailing_whitespace();
    void skip_to_line_number();
    void skip_to_line_with_string();
    void decode_utf16();

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
 * Per-column per-thread temporary string buffers used to assemble processed
 * string data. This buffer is used as a "staging ground" where the string data
 * is being stored / postprocessed before being transferred to the "main"
 * string buffer in a Column. Such 2-stage process is needed for the multi-
 * threaded string data writing.
 *
 * Members of this struct:
 *   .strdata -- memory region where the string data is stored.
 *   .allocsize -- allocation size of this memory buffer.
 *   .usedsize -- amount of memory already in use in the buffer.
 *   .writepos -- position in the global string data buffer where the current
 *       buffer's data should be moved. This value is returned from
 *       `WritableBuffer::prep_write()`.
 */
struct StrBuf2 {
  char* strdata;
  size_t allocsize;
  size_t usedsize;
  size_t writepos;
  int64_t colidx;

  StrBuf2(int64_t i);
  ~StrBuf2();
  void resize(size_t newsize);
};



/**
 * "Relative string": a string defined as an offset+length relative to some
 * anchor point (which has to be provided separately). This is the internal data
 * structure for reading strings from a file.
 */
struct RelStr {
  int32_t offset;
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
    field64* tbuf;
    size_t tbuf_ncols;
    size_t tbuf_nrows;
    size_t used_nrows;
    size_t row0;
    // std::vector<StrBuf2> strbufs;

  public:
    LocalParseContext(size_t ncols, size_t nrows);
    virtual ~LocalParseContext();
    virtual field64* next_row();
    virtual void push_buffers() = 0;
    virtual void read_chunk(const ChunkCoordinates&, ChunkCoordinates&) = 0;
    virtual void orderBuffer() = 0; // { row0 = r0; }
    virtual size_t get_nrows() { return used_nrows; }
    virtual void set_nrows(size_t n) { used_nrows = n; }

  public:
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
    size_t row0;
    size_t allocnrow;
    size_t max_nrows;
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

    /**
     * Return the fraction of the input that was parsed, as a number between
     * 0 and 1.0.
     */
    double work_done_amount() const;


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
     * Ensure that the chunks were placed properly. This method must be called
     * from the #ordered section. It takes two arguments: the *actual*
     * coordinates of the chunk just read; and the coordinates that were
     * *expected*. If the chunk was chunk was ordered properly, than this
     * method returns true. Otherwise, it updates the expected coordinates
     * `xcc` and returns false. The caller is expected to re-parse the chunk
     * with the updated coords, and then call this method again.
     */
    bool is_ordered(const ChunkCoordinates& acc, ChunkCoordinates& xcc);

    /**
     * This method can be overridden in derived classes in order to implement
     * more advanced chunk boundaries detection. This method will be called from
     * within `compute_chunk_boundaries()` only.
     * This method should modify `cc` by-reference; making sure not to alter
     * `start` / `end` if the flags `true_start` / `true_end` are set.
     */
    virtual void adjust_chunk_coordinates(
      ChunkCoordinates& cc, LocalParseContext* ctx) const;

    virtual LocalParseContextPtr init_thread_context() = 0;

  private:
    void determine_chunking_strategy();

    void realloc_output_columns(size_t i, size_t new_allocnrow);
};



#endif
