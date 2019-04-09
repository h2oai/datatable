//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include "csv/writer.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/string.h"
#include "options.h"
namespace py {




//------------------------------------------------------------------------------
// Frame::to_csv()
//------------------------------------------------------------------------------

static PKArgs args_to_csv(
    0, 1, 4, false, false,
    {"path", "nthreads", "hex", "verbose", "_strategy"},
    "to_csv",

R"(to_csv(self, path=None, nthreads=None, hex=False, verbose=False,
       _strategy="auto")
--

Write the Frame into the provided file in CSV format.

Parameters
----------
path: str
    Path to the output CSV file that will be created. If the file
    already exists, it will be overwritten. If no path is given,
    then the Frame will be serialized into a string, and that string
    will be returned.

nthreads: int
    How many threads to use for writing. The value of 0 means to use
    all available threads. Negative values indicate to use that many
    threads less than the maximum available. If this parameter is
    omitted then `dt.options.nthreads` will be used.

hex: bool
    If True, then all floating-point values will be printed in hex
    format (equivalent to %a format in C `printf`). This format is
    around 3 times faster to write/read compared to usual decimal
    representation, so its use is recommended if you need maximum
    speed.

verbose: bool
    If True, some extra information will be printed to the console,
    which may help to debug the inner workings of the algorithm.

_strategy: "mmap" | "write" | "auto"
    Which method to use for writing to disk. On certain systems 'mmap'
    gives a better performance; on other OSes 'mmap' may not work at
    all.
)");


oobj Frame::to_csv(const PKArgs& args)
{
  // path
  oobj path = args[0].to<oobj>(ostring(""));
  if (!path.is_string()) {
    throw TypeError() << "Parameter `path` in Frame.to_csv() should be a "
        "string, instead got " << path.typeobj();
  }
  path = oobj::import("os", "path", "expanduser").call({path});
  std::string filename = path.to_string();

  // nthreads
  int32_t maxth = static_cast<int32_t>(dt::num_threads_in_pool());
  int32_t nthreads = args[1].to<int32_t>(maxth);
  if (nthreads > maxth) nthreads = maxth;
  if (nthreads <= 0) nthreads += maxth;
  if (nthreads <= 0) nthreads = 1;

  // hex
  bool hex = args[2].to<bool>(false);

  // verbose
  bool verbose = args[3].to<bool>(false);
  oobj logger;
  if (verbose) {
    logger = oobj::import("datatable", "_DefaultLogger").call();
  }

  auto strategy = args[4].to<std::string>("");
  auto sstrategy = (strategy == "mmap")  ? WritableBuffer::Strategy::Mmap :
                   (strategy == "write") ? WritableBuffer::Strategy::Write :
                                           WritableBuffer::Strategy::Auto;

  // Create the CsvWriter object
  CsvWriter cwriter(dt, filename);
  cwriter.set_nthreads(static_cast<size_t>(nthreads));
  cwriter.set_strategy(sstrategy);
  cwriter.set_usehex(hex);
  cwriter.set_logger(logger);

  cwriter.write();

  // Post-process the result
  if (filename.empty()) {
    WritableBuffer* wb = cwriter.get_output_buffer();
    MemoryWritableBuffer* mb = dynamic_cast<MemoryWritableBuffer*>(wb);
    xassert(mb);

    // -1 because the buffer also stores trailing \0
    size_t len = mb->size() - 1;
    char* str = static_cast<char*>(mb->get_cptr());
    return ostring(str, len);
  }

  return None();
}




//------------------------------------------------------------------------------
// Declare Frame methods
//------------------------------------------------------------------------------

void Frame::Type::_init_tocsv(Methods& mm) {
  ADD_METHOD(mm, &Frame::to_csv, args_to_csv);

  init_csvwrite_constants();
}


}  // namespace py
