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
#include "csv/reader.h"             // GenericReader
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

static const char* doc_fread =
R"(fread(anysource=None, *, file=None, text=None, cmd=None, url=None,
         columns=None, sep=None, dec=".", max_nrows=None, header=None,
         na_strings=None, verbose=False, fill=False, encoding=None,
         skip_to_string=None, skip_to_line=0, skip_blank_lines=False,
         strip_whitespace=True, quotechar='"', tempdir=None,
         nthreads=None, logger=None, multiple_sources="warn",
         memory_limit=None)
--

This function is capable of reading data from a variety of input formats,
producing a :class:`Frame` as the result. The recognized formats are:
CSV, Jay, XLSX, and plain text. In addition, the data may be inside an
archive such as ``.tar``, ``.gz``, ``.zip``, ``.gz2``, and ``.tgz``.


Parameters
----------
anysource: str | bytes | file | Pathlike | List
    The first (unnamed) argument to fread is the *input source*.
    Multiple types of sources are supported, and they can be named
    explicitly: `file`, `text`, `cmd`, and `url`. When the source is
    not named, fread will attempt to guess its type. The most common
    type is `file`, but sometimes the argument is resolved as `text`
    (if the string contains newlines) or `url` (if the string starts
    with `https://` or similar).

    Only one argument out of `anysource`, `file`, `text`, `cmd` or
    `url` can be specified at once.

file: str | file | Pathlike
    A file source can be either the name of the file on disk, or a
    python "file-like" object -- i.e. any object having method
    ``.read()``.

    Generally, specifying a file name should be preferred, since
    reading from a Python ``file`` can only be done in single-threaded
    mode.

    This argument also supports addressing files inside an archive,
    or sheets inside an Excel workbook. Simply write the name of the
    file as if the archive was a folder: `"data.zip/train.csv"`.

text: str | bytes
    Instead of reading data from file, this argument provides the data
    as a simple in-memory blob.

cmd: str
    A command that will be executed in the shell and its output then
    read as text.

url: str
    This parameter can be used to specify the URL of the input file.
    The data will first be downloaded into a temporary directory and
    then read from there. In the end the temporary files will be
    removed.

    We use the standard ``urllib.request`` module to download the
    data. Changing the settings of that module, for example installing
    proxy, password, or cookie managers will allow you to customize
    the download process.

columns: ...
    Limit which columns to read from the input file.

sep: str | None
    Field separator in the input file. If this value is `None`
    (default) then the separator will be auto-detected. Otherwise it
    must be a single-character string. When ``sep='\n'``, then the
    data will be read in *single-column* mode. Characters
    ``["'`0-9a-zA-Z]`` are not allowed as the separator, as well as
    any non-ASCII characters.

dec: "." | ","
    Decimal point symbol for floating-point numbers.

max_nrows: int
    The maximum number of rows to read from the file. Setting this
    parameter to any negative number is equivalent to have no limit
    at all. Currently this parameter doesn't always work correctly.

header: bool | None
    If `True` then the first line of the CSV file contains the header.
    If `False` then there is no header. By default the presence of the
    header is heuristically determined from the contents of the file.

na_strings: List[str]
    The list of strings that were used in the input file to represent
    NA values.

fill: bool
    If `True` then the lines of the CSV file are allowed to have
    uneven number of fields. All missing fields will be filled with
    NAs in the resulting frame.

encoding: str | None
    If this parameter is provided, then the input will be recoded
    from this encoding into UTF-8 before reading. Any encoding
    registered with the python ``codec`` module can be used.

skip_to_string: str | None
    Start reading the file from the line containing this string. All
    previous lines will be skipped and discarded. This parameter
    cannot be used together with `skip_to_line`.

skip_to_line: int
    If this setting is given, then this many lines in the file will
    be skipped before we start to parse the file. This can be used
    for example when several first lines in the file contain non-CSV
    data and therefore must be skipped. This parameter cannot be
    used together with `skip_to_string`.

skip_blank_lines: bool
    If `True`, then any empty lines in the input will be skipped. If
    this parameter is `False` then: (a) in single-column mode empty
    lines are kept as empty lines; otherwise (b) if `fill=True` then
    empty lines produce a single line filled with NAs in the output;
    otherwise (c) an :exc:`dt.exceptions.IOError` is raised.

strip_whitespace: bool
    If `True`, then the leading/trailing whitespace will be stripped
    from unquoted string fields. Whitespace is always skipped from
    numeric fields.

quotechar: '"' | "'" | "`"
    The character that was used to quote fields in the CSV file. By
    default the double-quote mark `'"'` is assumed.

tempdir: str | None
    Use this directory for storing temporary files as needed. If not
    provided then the system temporary directory will be used, as
    determined via the :ext-mod:`tempfile` Python module.

nthreads: int | None
    Number of threads to use when reading the file. This number cannot
    exceed the number of threads in the pool ``dt.options.nthreads``.
    If `0` or negative number of threads is requested, then it will be
    treated as that many threads less than the maximum. By default
    all threads in the thread pool are used.

verbose: bool
    If `True`, then print detailed information about the internal
    workings of fread to stdout (or to `logger` if provided).

logger: object
    Logger object that will receive verbose information about fread's
    progress. When this parameter is specified, `verbose` mode will
    be turned on automatically.

multiple_sources: "warn" | "error" | "ignore"
    Action that should be taken when the input resolves to multiple
    distinct sources. By default, (`"warn"`) a warning will be issued
    and only the first source will be read and returned as a Frame.
    The `"ignore"` action is similar, except that the extra sources
    will be discarded without a warning. Lastly, an :exc:`dt.exceptions.IOError`
    can be raised if the value of this parameter is `"error"`.

    If you want all sources to be read instead of only the first one
    then consider using :func:`iread()`.

memory_limit: int
    Try not to exceed this amount of memory allocation (in bytes)
    when reading the data. This limit is advisory and not enforced
    very strictly.

    This setting is useful when reading data from a file that is
    substantially larger than the amount of RAM available on your
    machine.

    When this parameter is specified and fread sees that it needs
    more RAM than the limit in order to read the input file, then
    it will dump the data that was read so far into a temporary file
    in binary format. In the end the returned Frame will be partially
    composed from data located on disk, and partially from the data
    in memory. It is advised to either store this data as a Jay file
    or filter and materialize the frame (if not the performance may
    be slow).

return: Frame
    A single :class:`Frame` object is always returned.

    .. x-version-changed:: 0.11.0

        Previously, a ``dict`` of Frames was returned when multiple
        input sources were provided.

except: dt.exceptions.IOError

See Also
--------
- :func:`iread()`

)";

static py::PKArgs args_fread(
  1, 0, 23, false, false,
  {"anysource", "file", "text", "cmd", "url",
   "columns", "sep", "dec", "max_nrows", "header", "na_strings",
   "verbose", "fill", "encoding", "skip_to_string", "skip_to_line",
   "skip_blank_lines", "strip_whitespace", "quotechar",
   "tempdir", "nthreads", "logger", "multiple_sources", "memory_limit"
   },
  "fread", doc_fread);

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

static const char* doc_iread =
R"(iread(anysource=None, *, file=None, text=None, cmd=None, url=None,
         columns=None, sep=None, dec=".", max_nrows=None, header=None,
         na_strings=None, verbose=False, fill=False, encoding=None,
         skip_to_string=None, skip_to_line=None, skip_blank_lines=False,
         strip_whitespace=True, quotechar='"',
         tempdir=None, nthreads=None, logger=None, errors="warn",
         memory_limit=None)
--

This function is similar to :func:`fread()`, but allows reading
multiple sources at once. For example, this can be used when the
input is a list of files, or a glob pattern, or a multi-file archive,
or multi-sheet XLSX file, etc.


Parameters
----------
...: ...
    Most parameters are the same as in :func:`fread()`. All parse
    parameters will be applied to all input files.

errors: "warn" | "raise" | "ignore" | "store"
    What action to take when one of the input sources produces an
    error. Possible actions are: `"warn"` -- each error is converted
    into a warning and emitted to user, the source that produced the
    error is then skipped; `"raise"` -- the errors are raised
    immediately and the iteration stops; `"ignore"` -- the erroneous
    sources are silently ignored; `"store"` -- when an error is
    raised, it is captured and returned to the user, then the iterator
    continues reading the subsequent sources.

(return): Iterator[Frame] | Iterator[Frame|Exception]
    The returned object is an iterator that produces :class:`Frame` s.
    The iterator is lazy: each frame is read only as needed, after the
    previous frame was "consumed" by the user. Thus, the user can
    interrupt the iterator without having to read all the frames.

    Each :class:`Frame` produced by the iterator has a ``.source``
    attribute that describes the source of each frame as best as
    possible. Each source depends on the type of the input: either a
    file name, or a URL, or the name of the file in an archive, etc.

    If the `errors` parameter is `"store"` then the iterator may
    produce either Frames or exception objects.


See Also
--------
- :func:`fread()`
)";

static py::PKArgs args_iread(
  1, 0, 23, false, false,
  {"anysource", "file", "text", "cmd", "url",
   "columns", "sep", "dec", "max_nrows", "header", "na_strings",
   "verbose", "fill", "encoding", "skip_to_string", "skip_to_line",
   "skip_blank_lines", "strip_whitespace", "quotechar",
   "tempdir", "nthreads", "logger", "errors", "memory_limit"
   },
  "iread", doc_iread);

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
