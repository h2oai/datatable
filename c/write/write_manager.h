//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#ifndef dt_WRITE_WRITE_MANAGER_h
#define dt_WRITE_WRITE_MANAGER_h
#include <memory>
#include <vector>
#include "python/obj.h"
#include "write/column_builder.h"
#include "write/output_options.h"
#include "write/write_chronicler.h"
#include "write/writing_context.h"
#include "datatable.h"
#include "writebuf.h"
namespace dt {
namespace write {


class write_manager {
  protected:
    // Input parameters
    DataTable* dt;
    std::string path;
    output_options options;
    WritableBuffer::Strategy strategy;
    size_t : 56;

    // Runtime parameters
    write_chronicler chronicler;
    std::vector<column_builder> columns;
    std::unique_ptr<WritableBuffer> wb;
    size_t fixed_size_per_row;
    size_t estimated_output_size;
    size_t nchunks;
    py::oobj result;

    static constexpr size_t WRITE_PREPARE = 1;
    static constexpr size_t WRITE_MAIN = 100;
    static constexpr size_t WRITE_FINALIZE = 2;

  public:
    write_manager(DataTable* dt_, std::string path_);
    write_manager(const write_manager&) = delete;
    write_manager(write_manager&&) = delete;
    virtual ~write_manager();

    void set_strategy(WritableBuffer::Strategy);
    void set_logger(py::oobj logger);
    void set_usehex(bool);

    void write_main();
    py::oobj get_result();

  private:
    // Fills `columns` vector
    void create_column_writers();

    // Initializes the output buffer `wb`
    void create_output_target();

    // Computes variable `nchunks`
    void determine_chunking_strategy();

    // Write all data rows into the output
    void write_rows();

    // Close the output channel, and perform the necessary finalization steps.
    // Variable `result` is written.
    void finalize_output();

  protected:
    virtual std::string get_job_name() const = 0;

    // Computes `fixed_size_per_row` and `estimated_output_size`
    virtual void estimate_output_size() = 0;

    // Write whatever is needed before all the output rows
    virtual void write_preamble() = 0;

    // Write a single row `j` of the input DataTable into the output
    virtual void write_row(writing_context& ctx, size_t j) = 0;

    // Write the concluding section of the file, after the all rows
    virtual void write_epilogue() = 0;
};



}}  // namespace dt::write
#endif
