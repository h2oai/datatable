//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_CSV_WRITER_H
#define dt_CSV_WRITER_H
#include <stdbool.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "datatable.h"
#include "writebuf.h"
#include "utils.h"


class CsvColumn;

class CsvWriter {
  // Input parameters
  DataTable *dt;
  std::string path;
  std::vector<std::string> column_names;
  void *logger;
  int nthreads;
  WritableBuffer::Strategy strategy;
  bool usehex;
  bool verbose;
  __attribute__((unused)) char _padding[1];

  // Intermediate values used while writing the file
  std::unique_ptr<WritableBuffer> wb;
  size_t fixed_size_per_row;
  double rows_per_chunk;
  size_t bytes_per_chunk;
  int64_t nchunks;
  std::vector<CsvColumn*> columns;
  std::vector<CsvColumn*> strcolumns32;
  std::vector<CsvColumn*> strcolumns64;
  double t_last;
  double t_size_estimation;
  double t_create_target;
  double t_prepare_for_writing;
  double t_write_data;
  double t_finalize;

public:
  CsvWriter(DataTable *dt_, const std::string& path_);
  ~CsvWriter();

  void set_logger(void *v) { logger = v; }
  void set_nthreads(int n) { nthreads = n; }
  void set_usehex(bool v) { usehex = v; }
  void set_verbose(bool v) { verbose = v; }
  void set_strategy(WritableBuffer::Strategy s) { strategy = s; }
  void set_column_names(std::vector<std::string>& names) {
    column_names = std::move(names);
  }

  void write();
  WritableBuffer* get_output_buffer() { return wb.release(); }

private:
  double checkpoint();
  size_t estimate_output_size();
  void create_target(size_t size);
  void write_column_names();
  void determine_chunking_strategy(size_t size, int64_t nrows);
  void create_column_writers(size_t ncols);
};


void init_csvwrite_constants();

__attribute__((format(printf, 2, 3)))
void log_message(void *logger, const char *format, ...);


#endif
