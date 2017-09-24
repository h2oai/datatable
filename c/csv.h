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
#include <stdbool.h>
#include <stdint.h>
#include "datatable.h"
#include "memorybuf.h"


typedef struct CsvWriteParameters {

  // Output path where the DataTable should be written.
  const char *path;

  DataTable *dt;

  char **column_names;

  // PyObject that knows how to handle log messages
  void* logger;

  int nthreads;

  bool verbose;

  char _padding[3];

} CsvWriteParameters;


MemoryBuffer* csv_write(CsvWriteParameters *args);
void init_csvwrite_constants();

__attribute__((format(printf, 2, 3)))
void log_message(void *logger, const char *format, ...);
