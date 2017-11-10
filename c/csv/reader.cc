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
#include "csv/reader.h"
#include <stdint.h>
#include <stdlib.h>
#include "py_utils.h"
#include "utils/omp.h"

// Forward-declare utility functions
static int normalize_nthreads(const PyObj&);



//------------------------------------------------------------------------------

GenericReader::GenericReader(const PyObj& pyrdr)
    : nthreads(normalize_nthreads(pyrdr.attr("nthreads"))),
      verbose(pyrdr.attr("verbose").as_bool()),
      filename_arg(pyrdr.attr("filename")),
      text_arg(pyrdr.attr("text")),
      mbuf(nullptr)
{
  verbose = true;
}

GenericReader::~GenericReader() {
  if (mbuf) mbuf->release();
}

static int normalize_nthreads(const PyObj& nth) {
  int nthreads = static_cast<int>(nth.as_int64());
  int maxth = omp_get_max_threads();
  if (nthreads > maxth) nthreads = maxth;
  if (nthreads <= 0) nthreads += maxth;
  if (nthreads <= 0) nthreads = 1;
  return nthreads;
}



//------------------------------------------------------------------------------

void GenericReader::open_input() {
  const char* filename = filename_arg.as_cstring();
  const char* text = text_arg.as_cstring();
  if (text) {
    mbuf = new ExternalMemBuf(text);
  } else if (filename) {
    if (verbose) printf("  Opening file %s\n", filename);
    mbuf = new OvermapMemBuf(filename, 1);
    if (verbose) printf("  File opened, size: %zu\n", (mbuf->size() - 1));
  }
}

const char* GenericReader::dataptr() const {
  return static_cast<const char*>(mbuf->get());
}


std::unique_ptr<DataTable> GenericReader::read()
{
  open_input();

  {
    ArffReader arffreader(*this);
    auto dt = arffreader.read();
    if (dt) return dt;
  }

  return nullptr;
}
