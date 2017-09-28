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
#include <stdbool.h>
#include <stdint.h>
#include "datatable.h"
#include "memorybuf.h"


class CsvWriter {
  DataTable *dt;
  const char *path;
  char **column_names;
  void *logger;
  int nthreads;
  bool usehex;
  bool verbose;

public:
  CsvWriter(DataTable *dt_, const char *path_)
    : dt(dt_), path(path_), column_names(nullptr), logger(nullptr),
      nthreads(1), usehex(false), verbose(false) {}

  void set_column_names(char **names) { column_names = names; }
  void set_logger(void *v) { logger = v; }
  void set_nthreads(int n) { nthreads = n; }
  void set_usehex(bool v) { usehex = v; }
  void set_verbose(bool v) { verbose = v; }

  MemoryBuffer* write();
};


void init_csvwrite_constants();

__attribute__((format(printf, 2, 3)))
void log_message(void *logger, const char *format, ...);


#endif
