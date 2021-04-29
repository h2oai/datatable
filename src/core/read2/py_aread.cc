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
#include "python/xargs.h"
#include "read2/read_director.h"
#include "read2/read_options.h"
#include "read2/source_iterator.h"
namespace dt {
namespace read2 {


//------------------------------------------------------------------------------
// aread() python function
//------------------------------------------------------------------------------

static const char* doc_aread =
R"(aread(arg0, *, file=None, text=None, cmd=None, url=None, ...)
--

Parameters
----------
arg0: str | bytes | PathLike | file | List
    The first argument designates the **source** where the data should
    be read from. This argument can accommodate a variety of different
    sources, and `aread()` will attempt to guess the meaning of this
    argument based on its type and value.

    If you want to avoid possible ambiguities, another wait to specify
    the source(s) is to use one of the named arguments `file`, `text`,
    `url`, or `cmd`.

file: str | bytes | PathLike | file
    A file source can be either the name of a file on disk, or a
    python "file-like" object, i.e. any object having method `.read()`.

    Generally, specifying a file name should be preferred, since
    reading from a file object severely limits opportunities for
    multi-threading.

    This argument also supports addressing files inside an archive,
    or sheets inside an Excel workbook. Simply write the name of the
    file as if the archive was a folder: `"data.zip/train.csv"`.

text: str | bytes

cmd: str

url: str


See Also
--------
- :func:`iread()`
- :func:`fread()`
)";


static py::oobj aread(const py::XArgs& args) {
  size_t k = 0;
  auto arg0    = args[k++].to_robj();
  auto argFile = args[k++].to_robj();
  auto argText = args[k++].to_robj();
  auto argCmd  = args[k++].to_robj();
  auto argUrl  = args[k++].to_robj();

  const py::Arg& argLogger    = args[k++];
  const py::Arg& argVerbose   = args[k++];

  ReadOptions options;
  options.initLogger(argLogger, argVerbose);
  {
    auto section = options.logger().section("[0] Process input parameters");
  }

  auto sources = SourceIterator::fromArgs(
      "aread", arg0, argFile, argText, argCmd, argUrl
  );
  ReadDirector reader(std::move(sources), std::move(options));
  return reader.readSingle();
}


DECLARE_PYFN(&aread)
    ->name("aread")
    ->docs(doc_aread)
    ->n_positional_args(1)
    ->n_keyword_args(6)
    ->arg_names({"arg0", "file", "text", "cmd", "url",
                 "logger", "verbose"});




}}  // namespace dt::read
