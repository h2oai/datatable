//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "csv/reader.h"             // GenericReader
#include "documentation.h"
#include "python/args.h"            // py::PKArgs
#include "python/string.h"          // py::ostring
#include "read/multisource.h"       // MultiSource
#include "read/py_read_iterator.h"  // py::ReadIterator
#include "datatablemodule.h"        // ::DatatableModule
namespace dt {
namespace read {


//------------------------------------------------------------------------------
// fread() python function
//------------------------------------------------------------------------------

static py::PKArgs args_fread(
  1, 0, 23, false, false,
  {"anysource", "file", "text", "cmd", "url",
   "columns", "sep", "dec", "max_nrows", "header", "na_strings",
   "verbose", "fill", "encoding", "skip_to_string", "skip_to_line",
   "skip_blank_lines", "strip_whitespace", "quotechar",
   "tempdir", "nthreads", "logger", "multiple_sources", "memory_limit"
   },
  "fread", doc_dt_fread);

static py::oobj fread(const py::PKArgs& args) {
  size_t k = 5;  // skip source args for now
  const py::Arg& arg_columns    = args[k++];
  const py::Arg& arg_sep        = args[k++];
  const py::Arg& arg_dec        = args[k++];
  const py::Arg& arg_maxnrows   = args[k++];
  const py::Arg& arg_header     = args[k++];
  const py::Arg& arg_nastrings  = args[k++];
  const py::Arg& arg_verbose    = args[k++];
  const py::Arg& arg_fill       = args[k++];
  const py::Arg& arg_encoding   = args[k++];
  const py::Arg& arg_skiptostr  = args[k++];
  const py::Arg& arg_skiptoline = args[k++];
  const py::Arg& arg_skipblanks = args[k++];
  const py::Arg& arg_stripwhite = args[k++];
  const py::Arg& arg_quotechar  = args[k++];
  const py::Arg& arg_tempdir    = args[k++];
  const py::Arg& arg_nthreads   = args[k++];
  const py::Arg& arg_logger     = args[k++];
  const py::Arg& arg_multisrc   = args[k++];
  const py::Arg& arg_memlimit   = args[k++];

  GenericReader rdr;
  rdr.init_logger(arg_logger, arg_verbose);
  {
    auto section = rdr.logger_.section("[*] Process input parameters");
    rdr.init_nthreads(   arg_nthreads);
    rdr.init_fill(       arg_fill);
    rdr.init_maxnrows(   arg_maxnrows);
    rdr.init_skiptoline( arg_skiptoline);
    rdr.init_sep(        arg_sep);
    rdr.init_dec(        arg_dec);
    rdr.init_quote(      arg_quotechar);
    rdr.init_header(     arg_header);
    rdr.init_nastrings(  arg_nastrings);
    rdr.init_skipstring( arg_skiptostr);
    rdr.init_stripwhite( arg_stripwhite);
    rdr.init_skipblanks( arg_skipblanks);
    rdr.init_columns(    arg_columns);
    rdr.init_tempdir(    arg_tempdir);
    rdr.init_multisource(arg_multisrc);
    rdr.init_memorylimit(arg_memlimit);
    rdr.init_encoding(   arg_encoding);
  }

  MultiSource multisource(args, std::move(rdr));
  return multisource.read_single();
}



//------------------------------------------------------------------------------
// iread() python function
//------------------------------------------------------------------------------

static py::PKArgs args_iread(
  1, 0, 23, false, false,
  {"anysource", "file", "text", "cmd", "url",
   "columns", "sep", "dec", "max_nrows", "header", "na_strings",
   "verbose", "fill", "encoding", "skip_to_string", "skip_to_line",
   "skip_blank_lines", "strip_whitespace", "quotechar",
   "tempdir", "nthreads", "logger", "errors", "memory_limit"
   },
  "iread", doc_dt_iread);

static py::oobj iread(const py::PKArgs& args) {
  size_t k = 5;
  const py::Arg& arg_columns    = args[k++];
  const py::Arg& arg_sep        = args[k++];
  const py::Arg& arg_dec        = args[k++];
  const py::Arg& arg_maxnrows   = args[k++];
  const py::Arg& arg_header     = args[k++];
  const py::Arg& arg_nastrings  = args[k++];
  const py::Arg& arg_verbose    = args[k++];
  const py::Arg& arg_fill       = args[k++];
  const py::Arg& arg_encoding   = args[k++];
  const py::Arg& arg_skiptostr  = args[k++];
  const py::Arg& arg_skiptoline = args[k++];
  const py::Arg& arg_skipblanks = args[k++];
  const py::Arg& arg_stripwhite = args[k++];
  const py::Arg& arg_quotechar  = args[k++];
  const py::Arg& arg_tempdir    = args[k++];
  const py::Arg& arg_nthreads   = args[k++];
  const py::Arg& arg_logger     = args[k++];
  const py::Arg& arg_errors     = args[k++];
  const py::Arg& arg_memlimit   = args[k++];

  GenericReader rdr;
  rdr.init_logger(arg_logger, arg_verbose);
  {
    auto section = rdr.logger_.section("[*] Process input parameters");
    rdr.init_nthreads(   arg_nthreads);
    rdr.init_fill(       arg_fill);
    rdr.init_maxnrows(   arg_maxnrows);
    rdr.init_skiptoline( arg_skiptoline);
    rdr.init_sep(        arg_sep);
    rdr.init_dec(        arg_dec);
    rdr.init_quote(      arg_quotechar);
    rdr.init_header(     arg_header);
    rdr.init_nastrings(  arg_nastrings);
    rdr.init_skipstring( arg_skiptostr);
    rdr.init_stripwhite( arg_stripwhite);
    rdr.init_skipblanks( arg_skipblanks);
    rdr.init_columns(    arg_columns);
    rdr.init_tempdir(    arg_tempdir);
    rdr.init_errors(     arg_errors);
    rdr.init_memorylimit(arg_memlimit);
    rdr.init_encoding(   arg_encoding);
  }

  auto ms = std::make_unique<MultiSource>(args, std::move(rdr));
  return py::ReadIterator::make(std::move(ms));
}



}}  // namespace dt::read


void py::DatatableModule::init_methods_csv() {
  ADD_FN(&dt::read::fread, dt::read::args_fread);
  ADD_FN(&dt::read::iread, dt::read::args_iread);
}
