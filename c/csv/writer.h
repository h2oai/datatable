//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_CSV_H
#define dt_CSV_H
#include <string>
#include <vector>
#include <stdbool.h>
#include <stdint.h>
#include "datatable.h"
#include "memorybuf.h"
#include "utils.h"

class CsvColumn;

class CsvWriter {
  // Input parameters
  DataTable *dt;
  std::string path;
  std::vector<std::string> column_names;
  void *logger;
  int nthreads;
  bool usehex;
  bool verbose;
  __attribute__((unused)) char _padding[2];

  // Intermediate values used while writing the file
  WritableBuffer* wb;
  size_t fixed_size_per_row;
  double rows_per_chunk;
  size_t bytes_per_chunk;
  int64_t nchunks;
  std::vector<CsvColumn*> columns;
  std::vector<CsvColumn*> strcolumns;
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
  void set_column_names(std::vector<std::string>& names) {
    column_names = std::move(names);
  }

  void write();
  WritableBuffer* get_output_buffer() const { return wb; }

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
